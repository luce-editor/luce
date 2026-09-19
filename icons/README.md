# Custom Icons in Luce

Luce allows you to customize and extend file and folder icons.

## Adding Custom Icons

1. Place your `.svg` or `.png` icon files directly in this directory (`icons/`).
2. Open `icons.json` in Luce or any text editor.
3. Map your file extensions or exact filenames to your icon:

```json
{
  "extensions": {
    ".zig": "file_type_zig.svg",
    ".myext": "custom_icon.png"
  },
  "filenames": {
    "Dockerfile": "file_type_docker.svg",
    "SpecialFile.txt": "my_icon.svg"
  },
  "folders": {
    "default": "default_folder.svg",
    "default_open": "default_folder_opened.svg"
  }
}
```

4. Press `Ctrl+Shift+P` and run **Icons: Reload Icons** or click **Reload Icons** in Settings (*Appearance & Theme*).
