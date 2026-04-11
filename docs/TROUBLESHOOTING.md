# Troubleshooting

## Map Is Black or Style Fails With `401`

Symptoms:

- black map background
- log line such as `loading style failed: HTTP status code 401`
- warning about missing `MAPBOX_ACCESS_TOKEN` or `MAPBOX_STYLE_URL`

Check:

1. make sure `env` exists and contains valid values
2. run the app through `./scripts/run.sh`, or `source ./env` before launching manually
3. confirm the style URL is valid for the token you are using

Useful checks:

```bash
echo "$MAPBOX_ACCESS_TOKEN"
echo "$MAPBOX_STYLE_URL"
```

## `QMapLibre` Not Found During Configure

Symptoms:

- configure fails with a `QMapLibre` package error

Fix:

1. build MapLibre Native Qt:

   ```bash
   ./scripts/setup_maplibre_qt6.sh
   ```

2. if you installed it elsewhere, export:

   ```bash
   export MAPLIBRE_INSTALL_PREFIX=/path/to/install
   ```

## `qt-cmake` Not Found

The project expects an official Qt 6.11 install. Export `QT_ROOT` to the Qt `gcc_64` directory:

```bash
export QT_ROOT="$HOME/Qt/6.11.0/gcc_64"
```

Then use the provided scripts.

## OpenWeather Overlay Returns `401`

Symptoms:

- OpenWeather raster tiles fail
- click-based weather markers do not appear

Check:

1. `OPENWEATHERMAP_API_KEY` is set
2. the key is valid and active
3. the selected layer in `OPENWEATHERMAP_TILE_LAYER` is supported by your account path

The project currently uses the legacy tile endpoint because it is compatible with the verified working account flow.

## App Starts but Shows Repeated `QTimer::start` Warnings

This is a known legacy runtime issue in the current codebase. It does not block startup, but it indicates timer misuse in older subsystems that still need cleanup.

## Forced Exit Crashes With `free(): invalid pointer`

This is a known shutdown-path bug in the current application. The refactor and documentation work did not introduce it; it remains unresolved technical debt from the existing runtime stack.

## QML Lint Reports Many Warnings

The active Qt 6 runtime path is working, but some older QML files still contain:

- layout-positioning warnings
- unqualified-access warnings
- legacy signal/delegate patterns

These warnings are known and are being reduced incrementally.

