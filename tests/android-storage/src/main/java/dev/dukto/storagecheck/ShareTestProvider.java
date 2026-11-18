package dev.dukto.storagecheck;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.nio.charset.StandardCharsets;

/** Read-only provider used to test real URI grants and large multi-file shares. */
public final class ShareTestProvider extends ContentProvider {
    private File payload;
    public static final byte[] BYTES = "Dukto shared provider bytes\n".getBytes(StandardCharsets.UTF_8);
    @Override public boolean onCreate() {
        payload = new File(getContext().getCacheDir(), "share-payload.txt");
        try (FileOutputStream out = new FileOutputStream(payload)) { out.write(BYTES); }
        catch (Exception error) { return false; }
        return true;
    }
    @Override public String getType(Uri uri) { return "text/plain"; }
    @Override public Cursor query(Uri uri, String[] projection, String selection, String[] args, String order) {
        String[] columns = projection == null ? new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE} : projection;
        MatrixCursor result = new MatrixCursor(columns);
        Object[] values = new Object[columns.length];
        for (int i = 0; i < columns.length; ++i) {
            if (OpenableColumns.DISPLAY_NAME.equals(columns[i])) values[i] = "shared-" + uri.getLastPathSegment() + ".txt";
            else if (OpenableColumns.SIZE.equals(columns[i])) values[i] = BYTES.length;
        }
        result.addRow(values); return result;
    }
    @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) throw new FileNotFoundException("Read only");
        return ParcelFileDescriptor.open(payload, ParcelFileDescriptor.MODE_READ_ONLY);
    }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] args) { throw new UnsupportedOperationException(); }
    @Override public int delete(Uri uri, String selection, String[] args) { throw new UnsupportedOperationException(); }
}
