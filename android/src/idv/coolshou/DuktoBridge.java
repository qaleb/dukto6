package idv.coolshou;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.os.CancellationSignal;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import android.provider.OpenableColumns;

public class DuktoBridge {
    // Timeout provider calls even while the Qt UI thread is waiting for shutdown.
    private static final ScheduledThreadPoolExecutor watchdog = new ScheduledThreadPoolExecutor(1, task -> {
        Thread thread = new Thread(task, "Dukto-provider-timeout");
        thread.setDaemon(true);
        return thread;
    });
    static {
        watchdog.setKeepAliveTime(30, TimeUnit.SECONDS);
        watchdog.allowCoreThreadTimeOut(true);
        watchdog.setRemoveOnCancelPolicy(true);
    }
    // Ownership of the detached descriptor transfers to QFile on the I/O worker.
    public static String openSource(Context context, String value) {
        CancellationSignal cancellation = new CancellationSignal();
        java.util.concurrent.ScheduledFuture<?> timeout = watchdog.schedule(cancellation::cancel, 30, TimeUnit.SECONDS);
        AssetFileDescriptor asset = null;
        ParcelFileDescriptor descriptor = null;
        try {
            Uri uri = Uri.parse(value);
            String name = "Shared file";
            long size = -1;
            try (Cursor cursor = context.getContentResolver().query(uri,
                    new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE},null,null,null,cancellation)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (column >= 0 && !cursor.isNull(column)) name = cursor.getString(column);
                    column = cursor.getColumnIndex(OpenableColumns.SIZE);
                    if (column >= 0 && !cursor.isNull(column)) size = cursor.getLong(column);
                }
            }
            name = name.replaceAll("[\\x00-\\x1f\\\\/:<>\"|?*]", "_").replaceAll("[. ]+$", "");
            if (name.isEmpty() || name.equals(".") || name.equals("..") || name.equals("___DUKTO___TEXT___")) name = "Shared file";
            if (name.matches("(?i)^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\\..*)?$")) name = "Shared-" + name;
            asset = context.getContentResolver().openAssetFileDescriptor(uri,"r",cancellation);
            if (asset == null) return "";
            descriptor = asset.getParcelFileDescriptor();
            if (asset.getLength() >= 0) size = asset.getLength();
            return descriptor.detachFd() + "\n" + size + "\n" + name + "\n" + asset.getStartOffset();
        } catch (Exception error) {
            android.util.Log.w("Dukto","Shared file access expired or provider unavailable",error);
            return "";
        } finally {
            timeout.cancel(false);
            if (asset != null) try { asset.close(); } catch (Exception ignored) {}
            else if (descriptor != null) try { descriptor.close(); } catch (Exception ignored) {}
        }
    }
    public static void setTransferForeground(Context context, boolean active) {
        Runnable action = () -> {
            if (!active) {
                // Queue STOP after START. stopService() can destroy a tiny transfer's
                // pending foreground service before onStartCommand promotes it,
                // causing Android to kill the app a few seconds later.
                Intent stop = new Intent(context,DuktoTransferService.class);
                stop.setAction(DuktoTransferService.ACTION_STOP);
                try { context.startService(stop); }
                catch (RuntimeException error) {
                    android.util.Log.w("Dukto","Unable to stop transfer service",error);
                }
                return;
            }
            if (context instanceof Activity && Build.VERSION.SDK_INT >= 33 &&
                    context.checkSelfPermission("android.permission.POST_NOTIFICATIONS") != android.content.pm.PackageManager.PERMISSION_GRANTED)
                ((Activity)context).requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"},4645);
            Intent intent = new Intent(context,DuktoTransferService.class);
            intent.setAction(DuktoTransferService.ACTION_START);
            try {
                if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent);
                else context.startService(intent);
            } catch (RuntimeException error) {
                android.util.Log.w("Dukto","Foreground transfer service unavailable",error);
                DuktoActivity.nativeTimeout();
            }
        };
        if (context instanceof Activity) ((Activity)context).runOnUiThread(action); else action.run();
    }
    public static void openReceivedFile(Context context, String path) {
        Runnable open = () -> {
            try {
                Uri uri;
                if (path.startsWith("content://")) uri = Uri.parse(path);
                else {
                    java.io.File file = new java.io.File(path);
                    uri = androidx.core.content.FileProvider.getUriForFile(context,context.getPackageName()+".qtprovider",file);
                }
                String type = context.getContentResolver().getType(uri);
                Intent intent = new Intent(Intent.ACTION_VIEW).setDataAndType(uri,type == null ? "application/octet-stream" : type);
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            } catch (RuntimeException error) {
                android.widget.Toast.makeText(context,"No compatible app could open this file.",android.widget.Toast.LENGTH_LONG).show();
            }
        };
        if (context instanceof Activity) ((Activity)context).runOnUiThread(open); else open.run();
    }
    public static void openBatterySettings(Context context) {
        Intent intent = new Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.parse("package:" + context.getPackageName()));
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Runnable open = () -> context.startActivity(intent);
        if (context instanceof Activity) ((Activity)context).runOnUiThread(open); else open.run();
    }
}
