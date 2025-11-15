import com.github.scalerock.cjyaml.CJYaml;
import java.util.*;

public class ParsingFirstFile {
    public static void main(String[] args) {
        final String path = "example.yaml";

        try (CJYaml yaml = new CJYaml()) {
            yaml.parseFile(path, false);
            Object root = yaml.parseRoot();

            printYaml(root, 0);
        }
    }

    private static void printYaml(Object node, int indent) {
        if (node == null) {
            printIndent(indent);
            System.out.println("null");
            return;
        }

        // scalar
        if (node instanceof String || node instanceof Number || node instanceof Boolean) {
            printIndent(indent);
            System.out.println(node);
            return;
        }

        // list
        if (node instanceof List) {
            List<?> list = (List<?>) node;
            for (Object item : list) {
                if (item == null || item instanceof String || item instanceof Number || item instanceof Boolean) {
                    printIndent(indent);
                    System.out.println("- " + String.valueOf(item));
                } else {
                    printIndent(indent);
                    System.out.println("-");
                    printYaml(item, indent + 2);
                }
            }
            return;
        }

        // map
        if (node instanceof Map) {
            Map<?, ?> map = (Map<?, ?>) node;
            for (Map.Entry<?, ?> e : map.entrySet()) {
                Object key = e.getKey();
                Object value = e.getValue();

                printIndent(indent);

                if (key == null) key = "";
                String keyStr = String.valueOf(key);

                if (!keyStr.isEmpty()) {
                    System.out.print(keyStr + ":");
                    if (value == null || value instanceof String || value instanceof Number || value instanceof Boolean) {
                        System.out.println(" " + String.valueOf(value));
                    } else {
                        System.out.println();
                        printYaml(value, indent + 2);
                    }
                } else {
                    printYaml(value, indent);
                }
            }
            return;
        }

        // fallback
        printIndent(indent);
        System.out.println(String.valueOf(node));
    }

    private static void printIndent(int indent) {
        System.out.print(" ".repeat(indent));
    }
}
