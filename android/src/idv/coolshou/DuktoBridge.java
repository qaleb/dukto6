package idv.coolshou;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import java.util.ArrayList;
import java.util.List;

public class DuktoBridge {
    public static String consumePendingShareUris(Activity activity) {
        if (activity == null) {
            return "";
        }

        Intent intent = activity.getIntent();
        if (intent == null || intent.getAction() == null) {
            return "";
        }

        final String action = intent.getAction();
        List<String> uris = new ArrayList<>();

        if (Intent.ACTION_SEND.equals(action)) {
            Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                uris.add(uri.toString());
            }
        } else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
            ArrayList<Uri> list = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            if (list != null) {
                for (Uri uri : list) {
                    if (uri != null) {
                        uris.add(uri.toString());
                    }
                }
            }
        }

        // Clear processed intent so we don't re-import on next poll.
        if (!uris.isEmpty()) {
            activity.setIntent(new Intent());
            return String.join("\n", uris);
        }
        return "";
    }

    public static void setTransferForeground(Context context, boolean active) {
        if (context == null) {
            return;
        }
        Intent intent = new Intent(context, DuktoTransferService.class);
        if (active) {
            intent.setAction(DuktoTransferService.ACTION_START);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }
        } else {
            intent.setAction(DuktoTransferService.ACTION_STOP);
            context.startService(intent);
        }
    }
}
