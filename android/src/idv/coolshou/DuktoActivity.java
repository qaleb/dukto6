package idv.coolshou;

import android.content.ClipData;
import android.content.Context;
import android.app.Activity;
import android.database.Cursor;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.graphics.Rect;
import android.graphics.Point;
import android.view.View;
import android.view.WindowInsets;
import org.json.JSONArray;
import org.json.JSONObject;
import org.qtproject.qt.android.bindings.QtActivity;
import java.util.ArrayList;
import java.util.LinkedHashSet;

public class DuktoActivity extends QtActivity {
    private static final ArrayList<String> pending = new ArrayList<>();
    private static boolean nativeReady;
    public static native void nativeShare(String json);
    public static native void nativeTimeout();
    public static native void nativeStorageChanged();
    public static native void nativeKeyboardTop(int screenPixels);
    private static int keyboardTop = -1;
    private static final int RECEIVE_FOLDER = 4664;
    private static final int BROWSE_RECEIVED = 4665;

    public static void chooseReceiveFolder(Context context) {
        if (!(context instanceof Activity)) return;
        ((Activity)context).runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            ((Activity)context).startActivityForResult(intent, RECEIVE_FOLDER);
        });
    }
    public static void browseReceivedFiles(Context context) {
        if (!(context instanceof Activity)) return;
        ((Activity)context).runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*");
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            String selected = DuktoStorage.selectedTree(context);
            Uri initial = selected.isEmpty()
                    ? DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:Download/Dukto")
                    : Uri.parse(selected);
            if (Build.VERSION.SDK_INT >= 26) intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initial);
            ((Activity)context).startActivityForResult(intent, BROWSE_RECEIVED);
        });
    }
    @Override public void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (request == BROWSE_RECEIVED) { DuktoBridge.openReceivedFile(this, uri.toString()); return; }
        if (request != RECEIVE_FOLDER) return;
        try {
            int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            if (flags != (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION))
                throw new SecurityException("The selected folder must allow reading and writing.");
            getContentResolver().takePersistableUriPermission(uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            Uri document = DocumentsContract.buildDocumentUriUsingTree(uri, DocumentsContract.getTreeDocumentId(uri));
            String label = "Selected folder";
            try (Cursor cursor = getContentResolver().query(document, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) label = "Selected folder: " + cursor.getString(0);
            }
            // Keep old grants until restart: an active batch may still be writing to its captured folder.
            DuktoStorage.selectTree(this, uri.toString(), label);
            nativeStorageChanged();
        } catch (Exception error) {
            android.widget.Toast.makeText(this, "Choose a writable folder on the SD card.", android.widget.Toast.LENGTH_LONG).show();
        }
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        // Fullscreen Qt windows are not always resized by adjustResize. Observe
        // Android's visible frame without replacing Qt's own insets listener.
        View decor = getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(() -> {
            Rect frame = new Rect();
            decor.getWindowVisibleDisplayFrame(frame);
            Point display = new Point();
            getWindowManager().getDefaultDisplay().getRealSize(display);
            boolean visible = display.y - frame.bottom > 100 * getResources().getDisplayMetrics().density;
            int top = frame.bottom;
            if (Build.VERSION.SDK_INT >= 30) {
                WindowInsets insets = decor.getRootWindowInsets();
                if (insets != null) {
                    visible = insets.isVisible(WindowInsets.Type.ime());
                    top = Math.min(top, display.y - insets.getInsets(WindowInsets.Type.ime()).bottom);
                }
            }
            int next = visible ? top : -1;
            synchronized (DuktoActivity.class) {
                if (next != keyboardTop) {
                    keyboardTop = next;
                    if (nativeReady) nativeKeyboardTop(keyboardTop);
                }
            }
        });
        handleShare(getIntent());
        if (Build.VERSION.SDK_INT >= 37 && getApplicationInfo().targetSdkVersion >= 37 &&
            checkSelfPermission("android.permission.ACCESS_LOCAL_NETWORK") != android.content.pm.PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{"android.permission.ACCESS_LOCAL_NETWORK"},4644);
    }
    @Override public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleShare(intent);
    }
    public static synchronized void setNativeReady() {
        nativeReady = true;
        nativeKeyboardTop(keyboardTop);
        for (String value : pending) nativeShare(value);
        pending.clear();
    }
    public static synchronized void clearNativeReady() { nativeReady = false; }
    private void handleShare(Intent intent) {
        if (intent == null) return;
        String action = intent.getAction();
        if (!Intent.ACTION_SEND.equals(action) && !Intent.ACTION_SEND_MULTIPLE.equals(action)) return;
        try {
            LinkedHashSet<Uri> uris = new LinkedHashSet<>();
            if (Intent.ACTION_SEND.equals(action)) {
                Uri uri = Build.VERSION.SDK_INT >= 33 ? intent.getParcelableExtra(Intent.EXTRA_STREAM,Uri.class) : intent.getParcelableExtra(Intent.EXTRA_STREAM);
                if (uri != null) uris.add(uri);
            } else {
                ArrayList<Uri> list = Build.VERSION.SDK_INT >= 33 ? intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM,Uri.class) : intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (list != null) uris.addAll(list);
            }
            ClipData clip = intent.getClipData();
            if (clip != null) for (int i = 0; i < clip.getItemCount(); i++) {
                Uri uri = clip.getItemAt(i).getUri(); if (uri != null) uris.add(uri);
            }
            JSONArray sources = new JSONArray();
            for (Uri uri : uris) {
                if (uri == null || !("content".equals(uri.getScheme()) || "file".equals(uri.getScheme()))) continue;
                sources.put(uri.toString());
                if ((intent.getFlags() & Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION) != 0) {
                    try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); }
                    catch (SecurityException ignored) { /* Access may still be temporary. */ }
                }
            }
            CharSequence text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
            JSONObject payload = new JSONObject();
            payload.put("sources",sources);
            if (text != null) payload.put("text",text.toString());
            if (sources.length() == 0 && text == null) return;
            synchronized (DuktoActivity.class) {
                if (nativeReady) nativeShare(payload.toString()); else pending.add(payload.toString());
            }
            // Do not replace the Intent: its URI grant must remain available to active jobs.
            intent.setAction(null);
        } catch (Exception error) {
            android.util.Log.w("Dukto","Unable to import shared content",error);
        }
    }
}
