package idv.coolshou;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.app.PendingIntent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.Handler;
import android.os.Looper;

import androidx.core.app.NotificationCompat;

public class DuktoTransferService extends Service {
    public static final String ACTION_START = "idv.coolshou.action.TRANSFER_START";
    public static final String ACTION_STOP = "idv.coolshou.action.TRANSFER_STOP";
    private static final String CHANNEL_ID = "dukto_transfer_channel";
    private static final int NOTIF_ID = 4644;

    private PowerManager.WakeLock wakeLock;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable renewLock = new Runnable() {
        @Override public void run() {
            if (wakeLock != null) wakeLock.acquire(10 * 60 * 1000L);
            handler.postDelayed(this, 5 * 60 * 1000L);
        }
    };

    @Override public void onCreate() {
        super.onCreate();
        PowerManager power = (PowerManager)getSystemService(POWER_SERVICE);
        wakeLock = power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Dukto:Transfer");
        wakeLock.setReferenceCounted(false);
    }
    @Override public void onDestroy() {
        handler.removeCallbacksAndMessages(null);
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        stopForeground(true);
        super.onDestroy();
    }
    public void onTimeout(int startId, int type) {
        DuktoActivity.nativeTimeout();
        stopSelf();
    }
    @Override public void onTaskRemoved(Intent intent) {
        DuktoActivity.nativeTimeout();
        stopSelf();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent != null ? intent.getAction() : null;
        if (ACTION_STOP.equals(action)) {
            // Do not stop a newer START already queued by the next transfer.
            if (stopSelfResult(startId)) stopForeground(true);
            return START_NOT_STICKY;
        }

        createNotificationChannel();
        Intent open = new Intent(this, DuktoActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Dukto transfer in progress")
            .setContentText("Keeping transfer active")
            .setSmallIcon(android.R.drawable.stat_sys_upload)
            .setContentIntent(content)
            .setOnlyAlertOnce(true)
            .setOngoing(true)
            .build();
        if (Build.VERSION.SDK_INT >= 29)
            startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        else startForeground(NOTIF_ID, notification);
        handler.removeCallbacks(renewLock);
        renewLock.run();
        return START_NOT_STICKY;
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Dukto Transfers",
                NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
