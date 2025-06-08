# VRMotionToggle🎮 StreamerbotControl v1.1 - Production Release
Remote control ReShade effects via TCP commands with intelligent auto-restart
A robust ReShade addon that creates a TCP server allowing Streamerbot (and other applications) to remotely control visual effects in real-time. Perfect for interactive streaming, audience engagement, and automated visual experiences.

Remote control ReShade effects via TCP commands from Streamerbot with 
intelligent auto-restart

StreamerbotControl is a ReShade addon that creates a TCP server allowing 
external applications like Streamerbot to remotely control ReShade effects 
in real-time. Perfect for streamers who want their audience to control visual 
effects, create interactive experiences, or automate effect changes based on 
stream events.

License: MIT | ReShade: 5.0+ | Platform: Windows | Streamerbot: Compatible


                              KEY FEATURES

🌐 TCP Server
- Lightweight server listening on configurable port (default: 7777)

🤖 Streamerbot Integration  
- Direct compatibility with Streamerbot actions and commands

🔄 Auto-Restart
- Intelligent server monitoring with automatic restart on failure

💊 Health Monitoring
- Server heartbeat tracking and health status reporting

📊 Real-time Logging
- Comprehensive activity logging with timestamps

⚡ Low Latency
- Near-instant effect changes for responsive streaming

🛡️ Robust Error Handling
- Graceful handling of connection issues and network problems


                              USE CASES


Streaming & Content Creation:
- Audience Interaction: Let viewers control visual effects via chat commands
- Dynamic Scenes: Automatically change effects based on game events
- Show Production: Remote control effects during live productions
- Interactive Streams: Create engaging viewer participation experiences

Automation & Integration:
- OBS Integration: Trigger effects based on scene changes
- Alert Integration: Visual effects for follows, donations, subscriptions
- Game Integration: Effects triggered by in-game events
- Multi-App Control: Coordinate effects across multiple applications


                             INSTALLATION


Prerequisites:
- Windows 10/11
- ReShade 5.0 or later installed in your target application
- Streamerbot (or any TCP client application)
- Network access on chosen port (default: 7777)

Installation Steps:
1. Download the latest release from the Releases page
2. Extract StreamerbotControl.addon64 to your ReShade addons folder:
   [Game Directory]/StreamerbotControl.addon64
3. Launch your application with ReShade
4. Open the ReShade overlay (Home key by default)
5. Navigate to the "StreamerbotControl v1.1" tab
6. Click "Start Server" to begin listening for connections


                         STREAMERBOT INTEGRATION


Basic Setup:
1. Create a new Action in Streamerbot
2. Add an arguement to set the effectname
2. Add a "Execute C# Code" sub-action
3. Paste the integration code (see below)
4. Configure trigger (chat command, channel point reward, etc.)

Integration Code:

```csharp
using System;
using System.Net.Sockets;
using System.Text;

public class CPHInline
{
    public bool Execute()
    {
        // Get effect name from arguments or use default
        string effectName = args.ContainsKey("effectName") ? args["effectName"].ToString() : "MotionBlur";
        string action = args.ContainsKey("action") ? args["action"].ToString() : "TOGGLE";
        
        try
        {
            using (TcpClient client = new TcpClient())
            {
                // Connect to StreamerbotControl addon
                client.Connect("127.0.0.1", 7777);
                NetworkStream stream = client.GetStream();
                
                // Send command
                string command = $"{action} {effectName}\n";
                byte[] data = Encoding.ASCII.GetBytes(command);
                stream.Write(data, 0, data.Length);
                
                // Read response
                byte[] buffer = new byte[256];
                int bytes = stream.Read(buffer, 0, buffer.Length);
                string response = Encoding.ASCII.GetString(buffer, 0, bytes);
                
                CPH.SendMessage($"✅ ReShade: {response.Trim()}");
            }
        }
        catch (Exception ex)
        {
            CPH.SendMessage($"❌ ReShade Error: {ex.Message}");
            return false;
        }
        return true;
    }
}
```

Advanced Integration Examples:

Chat Command Integration:
```csharp
// For !blur command
string command = "TOGGLE MotionBlur\n";

// For !sharpen on/off commands  
string action = args["rawInput"].ToString().Contains("on") ? "ENABLE" : "DISABLE";
string command = $"{action} LumaSharpen\n";
```

Channel Points Reward:
```csharp
// Different effects for different rewards
string rewardName = args["rewardName"].ToString();
string command = "";

switch(rewardName.ToLower())
{
    case "motion blur":
        command = "TOGGLE MotionBlur\n";
        break;
    case "rainbow mode":
        command = "ENABLE ChromaticAberration\n";
        break;
    case "clean view":
        command = "DISABLE MotionBlur\n";
        break;
}
```


                            COMMAND REFERENCE


Command Format:
<ACTION> <technique_name>

Available Actions:
- TOGGLE: Switch effect on/off
- ENABLE / ON: Turn effect on
- DISABLE / OFF: Turn effect off

Example Commands:
TOGGLE MotionBlur
ENABLE ChromaticAberration  
DISABLE SMAA
ON LumaSharpen
OFF DepthOfField

Effect Name Matching:
- Exact Match: TOGGLE MotionBlur (exact technique name)
- Partial Match: TOGGLE Blur (matches any technique containing "Blur")
- Case Insensitive: Commands and effect names are case-insensitive

                             CONFIGURATION


Network Settings:
- Port: Default 7777 (configurable in ReShade overlay)
- Interface: Listens on all interfaces (0.0.0.0)
- Protocol: TCP with acknowledgment responses

Auto-Restart Settings:
Setting                 | Default    | Description
Enable Auto-Restart     | Enabled    | Automatically restart server on failure
Restart Delay           | 5 seconds  | Wait time between restart attempts
Max Attempts            | 10         | Maximum consecutive restart attempts

Advanced Options:
- Health Monitoring: 30-second heartbeat timeout
- Connection Timeout: Automatic client cleanup
- Buffer Size: 1024 bytes per command
- Log Retention: 100 most recent entries


                      MONITORING & DIAGNOSTICS


Server Status Indicators:
- 🟢 Running (Healthy): Server active and responsive
- 🔴 Running (Unhealthy): Server running but unresponsive  
- ⚫ Stopped: Server not running

Activity Log:
The built-in activity log shows:
- Server start/stop events
- Client connections and disconnections
- Commands received and processed
- Error messages and diagnostics
- Auto-restart attempts

Health Monitoring:
- Heartbeat Tracking: Server health checked every second
- Connection Monitoring: Client status and IP addresses
- Command Statistics: Total commands received counter
- Restart Tracking: Failed restart attempt counting


                            TROUBLESHOOTING


Common Issues:

"Server failed to start"
- Check if port 7777 is already in use
- Try a different port number
- Ensure Windows Firewall allows the connection

"Connection refused from Streamerbot"
- Verify server is running (check ReShade overlay)
- Confirm port numbers match
- Check if antivirus is blocking connections

"Effects not changing"
- Use "Show Available Techniques" to see exact effect names
- Try partial matching (e.g., "Blur" instead of "MotionBlur")
- Check ReShade overlay for error messages

"Server keeps restarting"
- Check Windows Firewall and antivirus settings
- Try disabling auto-restart temporarily
- Look for error patterns in the activity log

Debug Steps:
1. Check Server Status in ReShade overlay
2. Test with Telnet:
   telnet localhost 7777
   TOGGLE MotionBlur
3. Review Activity Log for error messages
4. Try Manual Restart using the "Restart Server" button
5. Verify Effect Names using "Show Available Techniques"


                      ALTERNATIVE CLIENT APPLICATIONS

While designed for Streamerbot, the TCP interface works with any application:

Python Example:
```python
import socket

def toggle_effect(effect_name):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect(('localhost', 7777))
        s.sendall(f'TOGGLE {effect_name}\n'.encode())
        response = s.recv(1024).decode()
        print(f"Response: {response}")

toggle_effect("MotionBlur")
```

PowerShell Example:
```powershell
$client = New-Object System.Net.Sockets.TcpClient
$client.Connect("127.0.0.1", 7777)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.WriteLine("TOGGLE MotionBlur")
$writer.Flush()
```

Command Line (Telnet):
telnet localhost 7777
ENABLE ChromaticAberration



                               LICENSE


This project is licensed under the MIT License


                           ACKNOWLEDGMENTS


- Streamerbot Community: For inspiration and testing
- ReShade Developers: For the excellent addon API
- Streaming Community: For creative use case ideas


                               SUPPORT


- Issues: Use the GitHub issue tracker
- Feature Requests: Open a discussion or issue
- Integration Help: Check the examples or ask in discussions



Made with ❤️ by LonelyViper


