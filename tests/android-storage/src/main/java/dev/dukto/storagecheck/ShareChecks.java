package dev.dukto.storagecheck;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import java.util.ArrayList;
import java.io.File;
import android.os.SystemClock;

/** Launch the actual Dukto share target from another UID, with real URI grants. */
public final class ShareChecks extends Instrumentation {
    private int count;
    private boolean chooser;
    private boolean hold;
    @Override public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        count = Integer.parseInt(arguments.getString("count", "500"));
        chooser = Boolean.parseBoolean(arguments.getString("chooser", "false"));
        hold = Boolean.parseBoolean(arguments.getString("hold", "false"));
        start();
    }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            File finished = new File(getTargetContext().getCacheDir(), "share-check-finish");
            finished.delete();
            if (count < 1 || count > 1000) throw new IllegalArgumentException("count must be 1..1000");
            ArrayList<Uri> uris = new ArrayList<>();
            for (int i = 0; i < count; ++i) uris.add(Uri.parse("content://dev.dukto.storagecheck.share/" + i));
            ClipData clip = new ClipData(new ClipDescription("Dukto share test", new String[]{"text/plain"}), new ClipData.Item(uris.get(0)));
            for (int i = 1; i < uris.size(); ++i) clip.addItem(new ClipData.Item(uris.get(i)));
            Intent intent = new Intent(count == 1 ? Intent.ACTION_SEND : Intent.ACTION_SEND_MULTIPLE);
            if (!chooser) intent.setClassName("org.qtproject.example.dukto6", "idv.coolshou.DuktoActivity");
            intent.setType("text/plain").setClipData(clip);
            if (count == 1) intent.putExtra(Intent.EXTRA_STREAM, uris.get(0));
            else intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_GRANT_READ_URI_PERMISSION);
            if (chooser) {
                Intent sheet = Intent.createChooser(intent, "Send with");
                sheet.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                getTargetContext().startActivity(sheet);
            } else getTargetContext().startActivity(intent);
            result.putInt("sharedFiles", count);
            sendStatus(1, result);
            // Finishing instrumentation force-stops its package and revokes URI
            // grants. Keep the sender alive until the receiving app has read them.
            if (hold) {
                long deadline = SystemClock.uptimeMillis() + 180000;
                while (!finished.exists() && SystemClock.uptimeMillis() < deadline) Thread.sleep(100);
            }
            finish(Activity.RESULT_OK, result);
        } catch (Throwable error) {
            result.putString("failure", android.util.Log.getStackTraceString(error));
            finish(Activity.RESULT_CANCELED, result);
        }
    }
}
