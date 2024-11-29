# WebAssembly C/C++ Audio Worklet Example

## Overview of Audio Worklet Architecture

1. **Thread Initialization** (once per application):
   - Create a dedicated audio processing thread
   - This single thread handles all audio processing for the entire application
   - Can only be initialized once per audio context

2. **Processor Registration** (once per processor type):
   - Register each unique audio worklet processor blueprint
   - Each processor type defines its own:
     - Parameters
     - Input/output configuration
     - Processing behavior
   - Multiple different processors can be registered within the same thread

3. **Node Creation** (multiple instances):
   - Create instances of any registered processor type
   - Each node instance maintains:
     - Its own independent state
     - Unique parameter values
     - Individual connections
   - Can create many nodes from the same processor type

In our case, we use this architecture to implement an oscillator bank, where:
- A single thread handles all audio processing
- One processor type defines our oscillator behavior
- Multiple oscillator nodes can be created and controlled independently

## Project Structure

### Source Files (Manual)
- `wasmOscBank.c` - Core C implementation of the oscillator bank
- `index.html` - User interface and controls
- `server.py` - Development server with required CORS headers for SharedArrayBuffer support

### Generated Files (Emscripten Output)
- `wasmOscBank.js` - JavaScript glue code
- `wasmOscBank.wasm` - WebAssembly binary
- `wasmOscBank.aw.js` - Audio Worklet processor code
- `wasmOscBank.ww.js` - Web Worker support code

## Prerequisites

- Emscripten SDK ([Installation Guide](https://emscripten.org/docs/getting_started/downloads.html))
- Python 3.x (for development server)
- Web browser with AudioWorklet support

## Building and Running

1. **Build the Project**:
```bash
emcc wasmOscBank.c -o wasmOscBank.js \
    -s WASM=1 \
    -s AUDIO_WORKLET=1 \
    -s WASM_WORKERS=1 \
    -s "EXPORTED_FUNCTIONS=['_startAudioWorkletThread', '_createAudioWorkletProcessor', '_connectAudioWorkletProcessor']" \
    -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']" \
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$emscriptenRegisterAudioObject, $emscriptenGetAudioObject'
```

2. **Start the Development Server**:
```bash
python3 server.py
```

3. **Access the Application**:
- Open `http://localhost:8080` in your web browser
- The custom server is required for SharedArrayBuffer support

## Features

- Dynamic creation/removal of oscillators
- Real-time frequency and volume control per oscillator
- Parameter smoothing for click-free audio
- Efficient WebAssembly-powered audio processing
- Cross-platform compatibility

## Implementation Details

### C Implementation (`wasmOscBank.c`)
- Defines oscillator state management and audio processing
- Implements parameter smoothing
- Handles WebAssembly/JavaScript bridge setup

### Audio Processing Flow
1. Thread initialization via `startAudioWorkletThread`
2. Processor registration via `createAudioWorkletProcessor`
3. Node creation and connection via `connectAudioWorkletProcessor`
4. Real-time audio processing in `ProcessAudio`

### Development Server (`server.py`)
```python
from http.server import HTTPServer, SimpleHTTPRequestHandler

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
    port = 8080
    httpd = HTTPServer(('localhost', port), CORSRequestHandler)
    print(f"Serving at http://localhost:{port}")
    httpd.serve_forever()
```

## Documentation References

- [Emscripten GitHub] (https://github.com/emscripten-core/emscripten/tree/main)
- [Web Audio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)
- [AudioWorklet Documentation](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet)
- [SharedArrayBuffer Requirements](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer#security_requirements)

## Browser Support

Requires a modern browser with support for:
- AudioWorklet API
- WebAssembly
- SharedArrayBuffer
- Cross-Origin Isolation

## Troubleshooting

1. **No Sound Output**:
   - Check browser console for errors
   - Verify audio context initialization
   - Ensure volume controls are not at zero

2. **Build Issues**:
   - Verify Emscripten SDK installation
   - Check all required flags in build command
   - Ensure source files are in correct locations

3. **CORS Errors**:
   - Use the provided Python server
   - Check browser console for specific CORS messages
   - Verify headers in network tab

## CORS Headers Configuration

For SharedArrayBuffer support, specific CORS headers are required. Example configurations are provided in the `headersForCORS` folder:

### Netlify
Use either `_headers`:
```text
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
```

Or `netlify.toml`:
```toml
[[headers]]
  for = "/*"
  [headers.values]
    Cross-Origin-Opener-Policy = "same-origin"
    Cross-Origin-Embedder-Policy = "require-corp"
```

### Development Server
The included Python server (`server.py`) already handles these headers. For other servers, ensure they send:
- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`

These headers are required for AudioWorklet functionality due to SharedArrayBuffer security requirements.
