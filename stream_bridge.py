"""
OSD-AUDIO — TikTok Live Bridge
Uso: python stream_bridge.py @tu_usuario

Escribe eventos JSON por línea a stdout para que OSD-AUDIO los lea.
Instalar: pip install TikTokLive
"""

import sys
import json

try:
    from TikTokLive import TikTokLiveClient
    from TikTokLive.events import (
        CommentEvent, GiftEvent, FollowEvent, LikeEvent,
        ConnectEvent, DisconnectEvent, LiveEndEvent,
    )
except ImportError:
    print(json.dumps({"type": "error", "message": "Instala TikTokLive: pip install TikTokLive"}), flush=True)
    sys.exit(1)


def emit(data: dict):
    print(json.dumps(data, ensure_ascii=False), flush=True)


def main():
    if len(sys.argv) < 2:
        emit({"type": "error", "message": "Falta nombre de usuario."})
        sys.exit(1)

    username = sys.argv[1]
    if not username.startswith('@'):
        username = '@' + username

    client = TikTokLiveClient(unique_id=username)

    @client.on(ConnectEvent)
    async def on_connect(event):
        emit({"type": "connected", "username": username})

    @client.on(DisconnectEvent)
    async def on_disconnect(event):
        emit({"type": "disconnected"})

    @client.on(LiveEndEvent)
    async def on_live_end(event):
        emit({"type": "disconnected"})

    def display_name(user):
        """Nombre público del usuario; si no hay, cae al @handle."""
        name = getattr(user, 'nickname', None) or getattr(user, 'display_name', None)
        return str(name) if name else str(getattr(user, 'unique_id', 'alguien'))

    @client.on(CommentEvent)
    async def on_comment(event):
        try:
            emit({
                "type":     "comment",
                "username": display_name(event.user),
                "text":     str(event.comment),
            })
        except Exception:
            pass

    @client.on(GiftEvent)
    async def on_gift(event):
        try:
            gift = event.gift
            is_over = (
                getattr(gift, 'is_streak_over', None)
                or getattr(gift, 'streak_over', None)
                or not getattr(gift, 'streakable', True)
                or True
            )
            if not is_over:
                return
            emit({
                "type":      "gift",
                "username":  display_name(event.user),
                "gift_name": str(getattr(gift, 'name', 'Regalo')),
                "amount":    int(getattr(gift, 'repeat_count', 1)),
            })
        except Exception:
            pass

    @client.on(FollowEvent)
    async def on_follow(event):
        try:
            emit({
                "type":     "follow",
                "username": display_name(event.user),
            })
        except Exception:
            pass

    @client.on(LikeEvent)
    async def on_like(event):
        try:
            emit({
                "type":     "like",
                "username": display_name(event.user),
                "count":    int(getattr(event, 'count', 1)),
            })
        except Exception:
            pass

    try:
        client.run()
    except KeyboardInterrupt:
        pass
    except Exception as e:
        msg = str(e).lower()
        if any(k in msg for k in ('not found', 'not live', '404', 'not currently live',
                                   'is offline', 'could not find')):
            emit({"type": "live_not_found",
                  "message": f"@{username.lstrip('@')} no está en vivo ahora"})
        # cualquier otro error: salir silenciosamente


if __name__ == "__main__":
    main()
