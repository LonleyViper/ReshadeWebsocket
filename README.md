# WebSocket Connector

A lightweight C# WebSocket client library designed for simple, real-time communication between your application and a WebSocket server.

> **Note:**  
> This version does **not** include any runtime environment modifications or hot-patching.

---

## Features

- Connects to any WebSocket server.
- Sends and receives string messages.
- Event-driven design for handling incoming messages and connection state.
- Minimal dependencies—easy to drop into most C# projects.

---

## Getting Started

### Prerequisites

- [.NET 6.0](https://dotnet.microsoft.com/en-us/download) or later  
  *(Project can be adapted for .NET Framework or earlier .NET Core if needed)*

### Installation

Clone this repository:

```bash
git clone https://github.com/yourusername/websocket-connector.git
cd websocket-connector
```

### Usage
Basic Example

```
using System;
using System.Threading.Tasks;

class Program
{
    static async Task Main()
    {
        var ws = new WebSocketClient("ws://localhost:8080");

        ws.OnMessageReceived += (sender, message) =>
        {
            Console.WriteLine("Received: " + message);
        };

        await ws.ConnectAsync();
        await ws.SendAsync("Hello, WebSocket!");
        
        // Keep running to receive messages
        Console.ReadLine();
        await ws.DisconnectAsync();
    }
}
```

### Contributing

    Fork the repository.

    Create your feature branch: git checkout -b feature/your-feature

    Commit your changes: git commit -am 'Add some feature'

    Push to the branch: git push origin feature/your-feature

    Open a pull request.

### License

This project is licensed under the MIT License. See LICENSE for details.
Disclaimer

This version is intended for use with compatible WebSocket servers and does not make runtime changes or require any additional runtime patching.


---

If you want to tweak the title, add badges, or customize anything, let me know!
