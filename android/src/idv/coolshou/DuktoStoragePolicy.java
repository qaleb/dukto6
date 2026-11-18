package idv.coolshou;

import java.nio.charset.StandardCharsets;
import java.util.Locale;

// Platform-independent policy; Android supplies the MIME type and performs the writes.
public final class DuktoStoragePolicy {
    public static final class Destination {
        public final String collection, directory, name;
        Destination(String collection, String directory, String name) {
            this.collection = collection; this.directory = directory; this.name = name;
        }
    }
    public static void validate(String relative) {
        if (relative == null || relative.isEmpty() || relative.startsWith("/")
                || relative.contains("\\") || relative.contains(":")
                || relative.getBytes(StandardCharsets.UTF_8).length > 4096)
            throw new IllegalArgumentException("Invalid received filename");
        for (String part : relative.split("/", -1)) {
            if (part.isEmpty() || part.equals(".") || part.equals("..") || part.endsWith(".") || part.endsWith(" "))
                throw new IllegalArgumentException("Invalid received path");
            for (int i = 0; i < part.length(); ++i)
                if (part.charAt(i) < 32 || "<>\"|?*".indexOf(part.charAt(i)) >= 0)
                    throw new IllegalArgumentException("Invalid received filename");
        }
    }
    public static Destination destination(String relative, String mime) {
        validate(relative);
        int slash = relative.lastIndexOf('/');
        String name = relative.substring(slash + 1);
        // A transferred folder stays together (HTML/assets, projects, etc.).
        if (slash >= 0) return new Destination("downloads", "Download/Dukto/" + relative.substring(0, slash) + "/", name);
        String type = mime == null ? "application/octet-stream" : mime.toLowerCase(Locale.ROOT);
        if (type.startsWith("image/")) return new Destination("images", "Pictures/Dukto/", name);
        if (type.startsWith("video/")) return new Destination("video", "Movies/Dukto/", name);
        if (type.startsWith("audio/")) return new Destination("audio", "Music/Dukto/", name);
        if (type.startsWith("text/") || type.equals("application/pdf") || type.equals("application/rtf")
                || type.equals("application/msword") || type.equals("application/json") || type.equals("application/xml")
                || type.equals("application/epub+zip") || type.startsWith("application/vnd.openxmlformats-officedocument.")
                || type.startsWith("application/vnd.oasis.opendocument.") || type.startsWith("application/vnd.ms-"))
            return new Destination("files", "Documents/Dukto/", name);
        return new Destination("downloads", "Download/Dukto/", name);
    }
}
