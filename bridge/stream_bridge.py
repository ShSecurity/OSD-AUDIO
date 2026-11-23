"""
OSD-AUDIO Stream Bridge
Captura eventos de TikTokLive y los retransmite via WebSocket local
al core C++/Qt en ws://localhost:8080
"""
import asyncio
import json
import websockets
from TikTokLive import TikTokLiveClient
from TikTokLive.events import CommentEvent, GiftEvent, LikeEvent, ConnectEvent

WS_PORT = 8080
TIKTOK_USERNAME = "@tu_usuario"  # Cambiar por el username de TikTok

connected_clients = set()

async def broadcast(event_data: dict):
    if connected_clients:
        message = json.dumps(event_data)
        await asyncio.gather(*[client.send(message) for client in connected_clients])

async def ws_handler(websocket):
    connected_clients.add(websocket)
    print(f"[OSD-Bridge] Cliente C++ conectado: {websocket.remote_address}")
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.discard(websocket)
        print(f"[OSD-Bridge] Cliente desconectado")

async def run_tiktok():
    client = TikTokLiveClient(unique_id=TIKTOK_USERNAME)

    @client.on(ConnectEvent)
    async def on_connect(event):
        print(f"[OSD-Bridge] Conectado a TikTok Live: {TIKTOK_USERNAME}")

    @client.on(CommentEvent)
    async def on_comment(event):
        await broadcast({
            "type": "comment",
            "username": event.user.unique_id,
            "message": event.comment
        })

    @client.on(GiftEvent)
    async def on_gift(event):
        await broadcast({
            "type": "donation",
            "username": event.user.unique_id,
            "gift": event.gift.name,
            "amount": event.gift.diamond_count
        })

    await client.start()

async def main():
    ws_server = await websockets.serve(ws_handler, "localhost", WS_PORT)
    print(f"[OSD-Bridge] WebSocket server en ws://localhost:{WS_PORT}")
    await asyncio.gather(ws_server.wait_closed(), run_tiktok())

if __name__ == "__main__":
    asyncio.run(main())
