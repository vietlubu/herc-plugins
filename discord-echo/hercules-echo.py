import discord
import asyncio
import socket
import struct

#################################################
### Constants
#################################################
DISCORD_BOT_TOKEN = 'discord_bot_token_here'
MAP_SERVER_IP = '127.0.0.1'
MAP_SERVER_PORT = 5121
WEBSERVER_IP = '127.0.0.1'    # Should not public ip
WEBSERVER_PORT = 5122     # Should not public port
### END Constants

#################################################
### Common variables
#################################################
game2discord_channels = {
    # "in-game channel": "discord channel id",
    "main": 1205170163368984597,
    "trade": 1205170179923775550,
    "support": 1205170214199894017,
}
discord2game_channels = {v: k for k, v in game2discord_channels.items()}
### END Common variables

#################################################
### Packet classes
#################################################
## Define discord message packet
class PacketDiscordMessage:
    def __init__(self, channel='main', username='Discord', message=''):
        self.packet_id = 0x0F01
        self.channel = channel
        self.username = username
        self.message = message

    def build(self):
        pkt_len = 2 + 24 + 24 + 250
        pkt_buf = bytearray(pkt_len)

        pkt_buf[0:2] = struct.pack('h', self.packet_id)
        pkt_buf[2:26] = struct.pack('<24s', self.channel.encode('utf-8'))
        pkt_buf[26:50] = struct.pack('<24s', self.username.encode('utf-8'))
        pkt_buf[50:300] = struct.pack('<250s', self.message.encode('utf-8'))

        return pkt_buf
## END Define discord message packet

## Define socket server connection
class SocketServer:
    def __init__(self, ip=MAP_SERVER_IP, port=MAP_SERVER_PORT):
        self.ip = ip
        self.port = port

    def send_packet(self, packet):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((self.ip, self.port))
                s.sendall(packet)
        except socket.error as e:
            print(f"Error connecting to {self.ip}:{self.port} - {e}")
## END Define socket server connection
## Create a network object
SocketServer = SocketServer()
### END Packet classes

#################################################
### Discord bot
#################################################
intents = discord.Intents.default()
intents.message_content = True

client = discord.Client(intents=intents)

@client.event
async def on_ready():
    print(f'We have logged in as {client.user}')

@client.event
async def on_message(message):
    if message.author.bot:
        return

    game_chanel = discord2game_channels.get(message.channel.id)
    username = "<" + message.author.name
    if message.author.discriminator and message.author.discriminator != '0':
        username += '#' + message.author.discriminator
    username += ">"
    
    packet = PacketDiscordMessage(channel=game_chanel, message=message.content, username=username)
    SocketServer.send_packet(packet.build())
### END Discord bot

#################################################
### Main. run the discord bot
#################################################
async def main():
    await asyncio.gather(
        client.start(DISCORD_BOT_TOKEN)
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except (asyncio.exceptions.CancelledError, KeyboardInterrupt):
        print("Task was cancelled or interrupted. Cleaning up...")
### END of the END
