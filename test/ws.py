import asyncio
import websockets

async def hello():
    async with websockets.connect("ws://120.76.98.140/wss") as websocket:
        name = input("请输入你的名字: ")
        await websocket.send(name)
        print(f">>> {name}")

        greeting = await websocket.recv()
        print(f"<<< {greeting}")

asyncio.run(hello())