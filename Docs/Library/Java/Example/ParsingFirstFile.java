import java.util.Map;
import com.github.scalerock.cjyaml.CJYaml;

public class ParsingFirstFile {
    public static void main(String[] args){
        final String path = "example.yaml";

        try (CJYaml yaml = new CJYaml()){
            yaml.parseFile(path);
            Map<?, ?> data = (Map<?, ?>) yaml.parseRoot();
            Object value = data.get("key");
            if (value instanceof String) {
                System.out.println(value);
            }

        }
    }
}