package idv.coolshou;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;
import org.json.JSONObject;
import java.io.IOException;
import java.io.File;
import java.util.Locale;
import java.util.UUID;
import java.util.HashSet;
import java.util.Set;
import android.content.Intent;

public final class DuktoStorage {
    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences("dukto-storage", Context.MODE_PRIVATE);
    }
    public static String selectedTree(Context context) { return preferences(context).getString("tree", ""); }
    public static String description(Context context) {
        if (!selectedTree(context).isEmpty()) return preferences(context).getString("label", "Selected SD-card folder");
        return Build.VERSION.SDK_INT >= 29 ? "Automatic: Gallery, Documents and Downloads" : "Choose a shared folder for Android 9";
    }
    public static void selectTree(Context context, String uri, String label) {
        Set<String> roots = new HashSet<>(preferences(context).getStringSet("receiveRoots", new HashSet<>()));
        if (!uri.isEmpty()) roots.add(uri);
        preferences(context).edit().putString("tree", uri).putString("label", label).putStringSet("receiveRoots", roots).apply();
    }
    private static String error(Exception exception) {
        android.util.Log.w("Dukto", "Shared storage operation failed", exception);
        try { return new JSONObject().put("error", exception.getMessage() == null ? "Storage unavailable. Select the SD-card folder again." : exception.getMessage()).toString(); }
        catch (Exception ignored) { return "{}"; }
    }
    private static String mime(String name) {
        int dot = name.lastIndexOf('.');
        String extension = dot < 0 ? "" : name.substring(dot + 1).toLowerCase(Locale.ROOT);
        String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
        if (type == null && (extension.equals("md") || extension.equals("log"))) type = "text/plain";
        return type == null ? "application/octet-stream" : type;
    }
    private static void track(Context context, Uri uri) throws IOException {
        if (!preferences(context).edit().putString("pending", uri.toString()).commit())
            throw new IOException("Could not record the pending received file");
    }
    private static void forget(Context context, String uri) {
        if (uri.equals(preferences(context).getString("pending", "")))
            preferences(context).edit().remove("pending").apply();
    }
    private static Uri insert(Context context, Uri collection, String directory, String name, String type) throws IOException {
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        values.put(MediaStore.MediaColumns.MIME_TYPE, type);
        values.put(MediaStore.MediaColumns.RELATIVE_PATH, directory);
        values.put(MediaStore.MediaColumns.IS_PENDING, 1);
        Uri uri = context.getContentResolver().insert(collection, values);
        if (uri == null) throw new IOException("Could not create a file in shared storage");
        return uri;
    }
    private static Uri collection(String kind) {
        if (Build.VERSION.SDK_INT < 29) throw new IllegalStateException("Automatic folders require Android 10 or later.");
        switch (kind) {
            case "images": return MediaStore.Images.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
            case "video": return MediaStore.Video.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
            case "audio": return MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
            case "files": return MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
            default: return MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
        }
    }
    private static String displayName(Context context, Uri uri, String fallback) {
        try (Cursor cursor = context.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) {}
        return fallback;
    }
    private static Uri child(Context context, Uri parent, String name) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(parent, DocumentsContract.getDocumentId(parent));
        try (Cursor cursor = context.getContentResolver().query(children,
                new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor != null) while (cursor.moveToNext())
                if (name.equalsIgnoreCase(cursor.getString(1)))
                    return DocumentsContract.buildDocumentUriUsingTree(parent, cursor.getString(0));
        }
        return null;
    }
    private static Uri parent(Context context, String tree, String relative) throws Exception {
        Uri root = Uri.parse(tree);
        Uri current = DocumentsContract.buildDocumentUriUsingTree(root, DocumentsContract.getTreeDocumentId(root));
        if (!relative.isEmpty()) for (String part : relative.split("/")) {
            Uri next = child(context, current, part);
            if (next == null) next = DocumentsContract.createDocument(context.getContentResolver(), current,
                    DocumentsContract.Document.MIME_TYPE_DIR, part);
            if (next == null) throw new IOException("Could not create a folder. Check the SD card and its permission.");
            current = next;
        }
        return current;
    }
    public static String beginFile(Context context, String relative, String tree) {
        Uri uri = null;
        try {
            DuktoStoragePolicy.validate(relative);
            String name = relative.substring(relative.lastIndexOf('/') + 1);
            String type = mime(name);
            String finalName = "";
            if (tree.isEmpty()) {
                if (Build.VERSION.SDK_INT < 29) throw new IOException("Choose a shared receive folder on Android 9.");
                DuktoStoragePolicy.Destination plan = DuktoStoragePolicy.destination(relative, type);
                uri = insert(context, collection(plan.collection), plan.directory, plan.name, type);
            } else {
                int slash = relative.lastIndexOf('/');
                Uri folder = parent(context, tree, slash < 0 ? "" : relative.substring(0, slash));
                finalName = new JSONObject().put("name", name).put("parent", folder.toString()).toString();
                // Incomplete files never masquerade as finished photos or documents.
                uri = DocumentsContract.createDocument(context.getContentResolver(), folder,
                        "application/octet-stream", ".dukto-" + UUID.randomUUID() + ".dukto-part");
                if (uri == null) throw new IOException("Could not create a file in the selected folder.");
                try (Cursor cursor = context.getContentResolver().query(uri,
                        new String[]{DocumentsContract.Document.COLUMN_FLAGS}, null, null, null)) {
                    int required = DocumentsContract.Document.FLAG_SUPPORTS_RENAME | DocumentsContract.Document.FLAG_SUPPORTS_DELETE;
                    if (cursor == null || !cursor.moveToFirst() || (cursor.getInt(0) & required) != required)
                        throw new IOException("This folder cannot safely finish received files. Choose a local or SD-card folder.");
                }
            }
            track(context, uri);
            try (ParcelFileDescriptor descriptor = context.getContentResolver().openFileDescriptor(uri, "w")) {
                if (descriptor == null) throw new IOException("Could not open the received file for writing.");
                JSONObject response = new JSONObject().put("uri", uri.toString())
                        .put("name", displayName(context, uri, name)).put("finalName", finalName);
                response.put("fd", descriptor.detachFd());
                return response.toString();
            }
        } catch (Exception failure) {
            if (uri != null) discard(context, uri.toString());
            return error(failure);
        }
    }
    public static String publish(Context context, String value, String finalName) {
        try {
            Uri uri = Uri.parse(value);
            String name;
            if ("media".equals(uri.getAuthority())) {
                ContentValues values = new ContentValues(); values.put(MediaStore.MediaColumns.IS_PENDING, 0);
                if (context.getContentResolver().update(uri, values, null, null) != 1)
                    throw new IOException("Could not publish the received file.");
                name = displayName(context, uri, "Received file");
            } else {
                // Document IDs are opaque; retain the provider's parent URI instead of parsing IDs.
                JSONObject target = new JSONObject(finalName);
                String requested = target.getString("name");
                Uri folder = Uri.parse(target.getString("parent"));
                String unique = requested;
                int dot = requested.lastIndexOf('.');
                String base = dot > 0 ? requested.substring(0, dot) : requested;
                String suffix = dot > 0 ? requested.substring(dot) : "";
                for (int n = 2; child(context, folder, unique) != null; ++n) unique = base + " (" + n + ")" + suffix;
                Uri renamed = DocumentsContract.renameDocument(context.getContentResolver(), uri, unique);
                if (renamed == null) throw new IOException("Could not finish the file in the selected folder.");
                uri = renamed;
                name = displayName(context, uri, unique);
                // ExternalStorageProvider indexes completed media after close/rename.
            }
            forget(context, value);
            return new JSONObject().put("uri", uri.toString()).put("name", name).toString();
        } catch (Exception failure) { return error(failure); }
    }
    public static void discard(Context context, String value) {
        try {
            Uri uri = Uri.parse(value);
            if ("media".equals(uri.getAuthority())) context.getContentResolver().delete(uri, null, null);
            else DocumentsContract.deleteDocument(context.getContentResolver(), uri);
            forget(context, value);
        } catch (Exception failure) { android.util.Log.w("Dukto", "Could not remove a pending received file", failure); }
    }
    public static String directory(Context context, String relative, String tree) {
        try {
            DuktoStoragePolicy.validate(relative);
            if (!tree.isEmpty()) parent(context, tree, relative);
            else {
                if (Build.VERSION.SDK_INT < 29) throw new IOException("Choose a shared receive folder on Android 9.");
                // MediaStore creates parents. A hidden pending marker materializes empty directories.
                Uri marker = insert(context, collection("downloads"), "Download/Dukto/" + relative + "/",
                        ".dukto-" + UUID.randomUUID(), "application/octet-stream");
                try {
                    track(context, marker);
                    try (ParcelFileDescriptor ignored = context.getContentResolver().openFileDescriptor(marker, "w")) {}
                } finally { discard(context, marker.toString()); }
            }
            return new JSONObject().put("ok", true).toString();
        } catch (Exception failure) { return error(failure); }
    }
    public static String reserveRootDirectory(Context context, String name, String tree) {
        try {
            DuktoStoragePolicy.validate(name);
            if (name.contains("/")) throw new IOException("Invalid root folder name");
            Uri root = tree.isEmpty() ? null : parent(context, tree, "");
            File downloads = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "Dukto");
            String unique = name;
            for (int n = 2; root == null ? new File(downloads, unique).exists() : child(context, root, unique) != null; ++n)
                unique = name + " (" + n + ")";
            JSONObject created = new JSONObject(directory(context, unique, tree));
            if (!created.optBoolean("ok")) return created.toString();
            return new JSONObject().put("name", unique).toString();
        } catch (Exception failure) { return error(failure); }
    }
    public static void recoverPending(Context context) {
        String value = preferences(context).getString("pending", "");
        if (value.isEmpty()) { releaseOldFolderGrants(context); return; }
        Uri uri = Uri.parse(value);
        try {
            if ("media".equals(uri.getAuthority()) && Build.VERSION.SDK_INT >= 29) {
                try (Cursor cursor = context.getContentResolver().query(uri,
                        new String[]{MediaStore.MediaColumns.IS_PENDING}, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst() && cursor.getInt(0) == 1) discard(context, value);
                    else forget(context, value); // Never delete a file published before a crash.
                }
            } else {
                try (Cursor cursor = context.getContentResolver().query(uri,
                        new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (cursor == null) return;
                    if (cursor.moveToFirst() && cursor.getString(0).endsWith(".dukto-part")) discard(context, value);
                    else forget(context, value);
                }
            }
        } catch (Exception failure) { android.util.Log.w("Dukto", "Pending storage cleanup deferred", failure); }
        finally { releaseOldFolderGrants(context); }
    }
    private static void releaseOldFolderGrants(Context context) {
        // Called at process startup, after pending-file recovery; never revoke an active batch's grant.
        Set<String> roots = new HashSet<>(preferences(context).getStringSet("receiveRoots", new HashSet<>()));
        String selected = selectedTree(context), pending = preferences(context).getString("pending", "");
        for (String root : new HashSet<>(roots)) {
            if (root.equals(selected) || pending.startsWith(root + "/")) continue;
            try {
                context.getContentResolver().releasePersistableUriPermission(Uri.parse(root),
                        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                roots.remove(root);
            } catch (SecurityException revoked) { roots.remove(root); }
        }
        preferences(context).edit().putStringSet("receiveRoots", roots).apply();
    }
}
