package dev.dukto.storagecheck;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsProvider;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;

// Deliberately opaque IDs catch any attempt to derive paths or parent IDs from document IDs.
public final class TestDocumentsProvider extends DocumentsProvider {
    private static final String[] COLUMNS = {"document_id", "_display_name", "mime_type", "flags", "_size"};
    private static final class Node {
        String id, parent, name, mime;
        File file;
        Node(String id, String parent, String name, String mime, File file) {
            this.id=id; this.parent=parent; this.name=name; this.mime=mime; this.file=file;
        }
    }
    private final Map<String,Node> nodes = new LinkedHashMap<>();
    @Override public boolean onCreate() {
        File root = new File(getContext().getFilesDir(), "documents"); root.mkdirs();
        nodes.put("opaque-root", new Node("opaque-root", "", "Test folder", DocumentsContract.Document.MIME_TYPE_DIR, root));
        return true;
    }
    private Node node(String id) throws FileNotFoundException {
        Node node=nodes.get(id); if(node==null) throw new FileNotFoundException(id); return node;
    }
    private void row(MatrixCursor cursor, Node node) {
        int flags=DocumentsContract.Document.FLAG_SUPPORTS_DELETE | DocumentsContract.Document.FLAG_SUPPORTS_RENAME;
        flags |= node.mime.equals(DocumentsContract.Document.MIME_TYPE_DIR)
                ? DocumentsContract.Document.FLAG_DIR_SUPPORTS_CREATE : DocumentsContract.Document.FLAG_SUPPORTS_WRITE;
        MatrixCursor.RowBuilder row=cursor.newRow();
        for(String column:cursor.getColumnNames()) {
            switch(column) {
                case "document_id": row.add(node.id); break;
                case "_display_name": row.add(node.name); break;
                case "mime_type": row.add(node.mime); break;
                case "flags": row.add(flags); break;
                case "_size": row.add(node.file.length()); break;
                default: row.add(null);
            }
        }
    }
    @Override public Cursor queryRoots(String[] projection) {
        return new MatrixCursor(projection==null ? new String[]{"root_id","document_id","title","flags"} : projection);
    }
    @Override public synchronized Cursor queryDocument(String id,String[] projection) throws FileNotFoundException {
        MatrixCursor cursor=new MatrixCursor(projection==null ? COLUMNS : projection); row(cursor,node(id)); return cursor;
    }
    @Override public synchronized Cursor queryChildDocuments(String id,String[] projection,String sort) {
        MatrixCursor cursor=new MatrixCursor(projection==null ? COLUMNS : projection);
        for(Node node:nodes.values()) if(node.parent.equals(id)) row(cursor,node);
        return cursor;
    }
    @Override public synchronized boolean isChildDocument(String parent,String id) {
        for(Node node=nodes.get(id);node!=null;node=nodes.get(node.parent)) if(node.id.equals(parent)) return true;
        return false;
    }
    @Override public synchronized String createDocument(String parent,String mime,String name) throws FileNotFoundException {
        Node folder=node(parent);
        String id="opaque-"+UUID.randomUUID(); File file=new File(folder.file,id);
        try {
            if(mime.equals(DocumentsContract.Document.MIME_TYPE_DIR)) file.mkdir(); else file.createNewFile();
        } catch(Exception error) { throw new FileNotFoundException(error.toString()); }
        nodes.put(id,new Node(id,parent,name,mime,file)); return id;
    }
    @Override public synchronized String renameDocument(String id,String name) throws FileNotFoundException {
        Node changed=node(id);
        for(Node node:nodes.values())
            if(!node.id.equals(id) && node.parent.equals(changed.parent) && node.name.equalsIgnoreCase(name))
                throw new FileNotFoundException("Refusing to overwrite a sibling");
        changed.name=name; return id;
    }
    @Override public synchronized void deleteDocument(String id) throws FileNotFoundException {
        Node deleted=node(id); deleted.file.delete(); nodes.remove(id);
    }
    @Override public synchronized ParcelFileDescriptor openDocument(String id,String mode,CancellationSignal signal) throws FileNotFoundException {
        return ParcelFileDescriptor.open(node(id).file,ParcelFileDescriptor.parseMode(mode));
    }
}
