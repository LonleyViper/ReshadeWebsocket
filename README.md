# WebSocket Connector

A lightweight C# WebSocket client library designed for simple, real-time communication between an application like Streamerbot and a WebSocket server.

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

### Usage in Streamerbot

Basic Example of c# code in streamerbot to turn an effect ON.

Set argument in streamerbot to the filter name.

You can open the plugin in reshade to see a list of available filters - click to copy the name!

Sample import code for streamerbot included.

```
using System;
using System.Net.Sockets;
using System.Text;

public class CPHInline
{
    public bool Execute()
    {
        // You can pass effectName as an argument/variable from Streamer.bot
        string effectName = args.ContainsKey("effectName") ? args["effectName"].ToString() : "MotionBlur";

        try
        {
            using (TcpClient client = new TcpClient())
            {
                client.Connect("127.0.0.1", 7777);
                NetworkStream stream = client.GetStream();

                string command = $"ON {effectName}\n";
                byte[] data = Encoding.ASCII.GetBytes(command);
                stream.Write(data, 0, data.Length);

                // Read response
                byte[] buffer = new byte[256];
                int bytes = stream.Read(buffer, 0, buffer.Length);
                string response = Encoding.ASCII.GetString(buffer, 0, bytes);

                CPH.SendMessage($"ReShade response: {response}");
            }
        }
        catch (Exception ex)
        {
            CPH.SendMessage($"Error: {ex.Message}");
            return false;
        }

        return true;
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
