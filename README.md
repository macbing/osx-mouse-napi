# osx-mouse

Mouse tracking for macOS. Receive the screen position of mouse events, including while another application is in the foreground. Events are observed only and are not consumed.

Requires **macOS 10.15** or later and **Node.js 16** or later. The native addon is built with [Node-API](https://nodejs.org/api/n-api.html) (`node-addon-api`) and also uses libuv, so `npm install` compiles it for the Node.js version you are running. Xcode command line tools are required.

	npm install osx-mouse

# Usage

The module returns an event emitter instance.

```javascript
var mouse = require('osx-mouse')()

mouse.on('move', function(x, y) {
	console.log(x, y)
})
```

The program will not terminate as long as a mouse listener is active. To allow the program to exit, either call `mouse.unref` (works as `unref`/`ref` on a TCP server) or `mouse.destroy()`.

The events emitted are: `move`, `left-down`, `left-up`, `left-drag`, `right-down`, `right-up`, and `right-drag`. For each event the screen coordinates are passed to the handler function.

# Limitations

From *macOS Mojave* and forward, mouse events are only delivered after the process is allowed under **Accessibility** (辅助功能). Input Monitoring is not required.

When running from *Terminal*:

1. Open `System Settings > Privacy & Security > Accessibility`
2. Add *Terminal* (or the app that launches Node) to the list and turn it on

On macOS Catalina and earlier, the same list is under `System Preferences > Security & Privacy > Privacy > Accessibility`. Restart the process after changing the permission.
