package dev.dukto.storagecheck;

import android.app.Instrumentation;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import idv.coolshou.DuktoStorage;
import org.json.JSONObject;
import java.io.File;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.UUID;

public final class StorageChecks extends Instrumentation {
    private Context context;
    private int checks;
    private final ArrayList<Uri> cleanup = new ArrayList<>();
    private final String prefix="dukto-check-"+UUID.randomUUID();
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    private void check(boolean value,String message) { if(!value) throw new AssertionError(message); ++checks; }
    private JSONObject begin(String name,String tree) throws Exception {
        JSONObject file=new JSONObject(DuktoStorage.beginFile(context,name,tree));
        check(file.has("fd"),"beginFile failed: "+file);
        Uri uri=Uri.parse(file.getString("uri")); cleanup.add(uri);
        return file;
    }
    private void write(JSONObject file,byte[] bytes) throws Exception {
        try(OutputStream out=new ParcelFileDescriptor.AutoCloseOutputStream(ParcelFileDescriptor.adoptFd(file.getInt("fd")))) {
            out.write(bytes);
        }
    }
    private JSONObject publish(JSONObject file) throws Exception {
        JSONObject done=new JSONObject(DuktoStorage.publish(context,file.getString("uri"),file.getString("finalName")));
        check(done.has("uri"),"publish failed: "+done);
        cleanup.add(Uri.parse(done.getString("uri"))); return done;
    }
    private int pending(Uri uri) {
        try(Cursor cursor=context.getContentResolver().query(uri,new String[]{MediaStore.MediaColumns.IS_PENDING},null,null,null)) {
            return cursor!=null && cursor.moveToFirst() ? cursor.getInt(0) : -1;
        } catch(Exception absent) { return -1; }
    }
    private String column(Uri uri,String name) {
        try(Cursor cursor=context.getContentResolver().query(uri,new String[]{name},null,null,null)) {
            return cursor!=null && cursor.moveToFirst() ? cursor.getString(0) : "";
        } catch(Exception absent) { return ""; }
    }
    private void route(String filename,String directory) throws Exception {
        JSONObject file=begin(filename,""); Uri uri=Uri.parse(file.getString("uri"));
        check(pending(uri)==1,"An incomplete item was published");
        check(directory.equals(column(uri,MediaStore.MediaColumns.RELATIVE_PATH)),"Incorrect destination for "+filename);
        write(file,"complete".getBytes("UTF-8")); publish(file);
        check(pending(uri)==0,"Completed item remains hidden");
    }
    @Override public void onStart() {
        Bundle result=new Bundle(); int status=-1; context=getTargetContext();
        try {
            route(prefix+".jpg","Pictures/Dukto/");
            route(prefix+".mp4","Movies/Dukto/");
            route(prefix+".mp3","Music/Dukto/");
            route(prefix+".pdf","Documents/Dukto/");
            route(prefix+".zip","Download/Dukto/");
            route(prefix+"/assets/photo.jpg","Download/Dukto/"+prefix+"/assets/");
            JSONObject cancelled=begin(prefix+"-cancel.jpg",""); write(cancelled,new byte[]{1,2});
            Uri cancelledUri=Uri.parse(cancelled.getString("uri")); DuktoStorage.discard(context,cancelledUri.toString());
            check(pending(cancelledUri)==-1,"Cancelled media remains in the store");
            JSONObject stale=begin(prefix+"-stale.jpg",""); write(stale,new byte[]{1,2});
            Uri staleUri=Uri.parse(stale.getString("uri")); DuktoStorage.recoverPending(context);
            check(pending(staleUri)==-1,"A crash left a pending item behind");
            JSONObject good=begin(prefix+"-good.txt",""); write(good,new byte[]{1,2}); publish(good);
            context.getSharedPreferences("dukto-storage",0).edit().putString("pending",good.getString("uri")).commit();
            DuktoStorage.recoverPending(context);
            check(pending(Uri.parse(good.getString("uri")))==0,"Recovery deleted a completed file");
            JSONObject dir=new JSONObject(DuktoStorage.directory(context,prefix+"/empty",""));
            check(dir.optBoolean("ok"),"Could not create an empty folder: "+dir);
            check(new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),"Dukto/"+prefix+"/empty").isDirectory(),"Empty folder was not materialized");
            JSONObject duplicateRoot=new JSONObject(DuktoStorage.reserveRootDirectory(context,prefix,""));
            check((prefix+" (2)").equals(duplicateRoot.optString("name")),"A repeated folder would merge with an earlier transfer");
            String tree=DocumentsContract.buildTreeDocumentUri("dev.dukto.storagecheck.documents","opaque-root").toString();
            JSONObject cardRoot=new JSONObject(DuktoStorage.reserveRootDirectory(context,"project",tree));
            check("project".equals(cardRoot.optString("name")),"Could not reserve a SAF folder");
            JSONObject duplicateCardRoot=new JSONObject(DuktoStorage.reserveRootDirectory(context,"project",tree));
            check("project (2)".equals(duplicateCardRoot.optString("name")),"Repeated SAF folder was not renamed");
            JSONObject first=begin("nested/same.txt",tree); write(first,"first".getBytes("UTF-8")); JSONObject one=publish(first);
            JSONObject second=begin("nested/same.txt",tree); write(second,"second".getBytes("UTF-8")); JSONObject two=publish(second);
            check(!one.getString("uri").equals(two.getString("uri")),"Duplicate names overwrote an existing document");
            check("same.txt".equals(column(Uri.parse(one.getString("uri")),OpenableColumns.DISPLAY_NAME)),"Original document was renamed");
            check("same (2).txt".equals(column(Uri.parse(two.getString("uri")),OpenableColumns.DISPLAY_NAME)),"Duplicate name was not resolved");
            JSONObject partial=begin("nested/photo.png",tree); write(partial,new byte[]{1});
            check(column(Uri.parse(partial.getString("uri")),OpenableColumns.DISPLAY_NAME).endsWith(".dukto-part"),"Incomplete SAF file looks complete");
            DuktoStorage.recoverPending(context);
            check(column(Uri.parse(partial.getString("uri")),OpenableColumns.DISPLAY_NAME).isEmpty(),"SAF recovery did not remove its partial file");
            result.putString("summary",checks+" Android storage checks passed");
        } catch(Throwable failure) {
            result.putString("failure",android.util.Log.getStackTraceString(failure)); status=0;
        } finally {
            for(Uri uri:cleanup) try {
                if("media".equals(uri.getAuthority())) context.getContentResolver().delete(uri,null,null);
                else DocumentsContract.deleteDocument(context.getContentResolver(),uri);
            } catch(Exception ignored) {}
        }
        finish(status,result);
    }
}
