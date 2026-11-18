package idv.coolshou;

public final class StoragePolicyTest {
    private static int checks;
    private static void check(String name, String mime, String collection, String directory) {
        DuktoStoragePolicy.Destination destination = DuktoStoragePolicy.destination(name, mime);
        if (!collection.equals(destination.collection) || !directory.equals(destination.directory))
            throw new AssertionError(name + " was routed to " + destination.collection + ":" + destination.directory);
        ++checks;
    }
    private static void rejects(String path) {
        try { DuktoStoragePolicy.validate(path); }
        catch (IllegalArgumentException expected) { ++checks; return; }
        throw new AssertionError("Unsafe path accepted: " + path);
    }
    public static void main(String[] args) {
        check("photo.jpg", "image/jpeg", "images", "Pictures/Dukto/");
        check("写真.png", "image/png", "images", "Pictures/Dukto/");
        check("movie.mp4", "video/mp4", "video", "Movies/Dukto/");
        check("voice.ogg", "audio/ogg", "audio", "Music/Dukto/");
        check("report.pdf", "application/pdf", "files", "Documents/Dukto/");
        check("report.docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", "files", "Documents/Dukto/");
        check("sheet.xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "files", "Documents/Dukto/");
        check("notes.md", "text/plain", "files", "Documents/Dukto/");
        check("book.epub", "application/epub+zip", "files", "Documents/Dukto/");
        check("archive.zip", "application/zip", "downloads", "Download/Dukto/");
        check("unknown.bin", null, "downloads", "Download/Dukto/");
        check("project/assets/photo.jpg", "image/jpeg", "downloads", "Download/Dukto/project/assets/");
        check("project/document.pdf", "application/pdf", "downloads", "Download/Dukto/project/");
        for (String path : new String[]{"", "/escape", "../escape", "folder/../escape", "folder//file", "folder/", "C:\\escape", "foo:bar", "bad\u0000name", "bad?name", "folder/.."}) rejects(path);
        System.out.println(checks + " storage policy checks passed");
    }
}
