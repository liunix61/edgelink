#!/bin/python3

import asyncio
import json, time
from aiomqtt import Client, Message


BROKER_ADDRESS = "test.mosquitto.org"

PROJECT_ID = "yncic-dangerous"

TOPIC_EXT_NTP_REQUEST = (
    f"/ext/ntp/{PROJECT_ID}/3D24E79E3D7D4078B48440B528E7BF74/request"
)
TOPIC_EXT_NTP_RESPONSE = (
    f"/ext/ntp/{PROJECT_ID}/3D24E79E3D7D4078B48440B528E7BF74/response"
)

DATA_TOPIC1 = f"/{PROJECT_ID}/data/test1"
TEST_TOPIC1 = f"/{PROJECT_ID}/3D24E79E3D7D4078B48440B528E7BF74/test1"
TEST_TOPIC2 = f"/{PROJECT_ID}/3D24E79E3D7D4078B48440B528E7BF74/test2"


def now_ms():
    return int(round(time.time() * 1000.0))


async def handle_ext_ntp(client: Client, message: Message):
    server_recv_time = now_ms()
    req = json.load(message.payload)
    # {"deviceSendTime":"1592361428000"}
    device_send_time = req["deviceSendTime"]
    # 回发时间给客户端:
    # {"deviceSendTime":"1592361428000","serverSendTime":"1592366463548","serverRecvTime":"1592366463548"}
    # 计算公式：
    # (serverRecvTime + serverSendTime + deviceRecvTime - deviceSendTime) / 2
    rep_json = {
        "deviceSendTime": device_send_time,
        "serverSendTime": now_ms(),
        "serverRecvTime": server_recv_time,
    }
    rep = json.dumps(rep_json)
    await client.publish(TOPIC_EXT_NTP_RESPONSE, rep, 1)


async def main():
    print("开始连接 Broker...")
    async with Client(BROKER_ADDRESS) as client:
        print("Broker 连接成功")

        async with client.messages() as messages:
            print("开始订阅消息...")
            await client.subscribe(DATA_TOPIC1, 0)
            await client.subscribe(TEST_TOPIC1)
            await client.subscribe(TEST_TOPIC2)

            print("开始消息循环...")
            async for message in messages:
                print(f"已接受到消息（topic=`{message.topic}`）：")
                if message.topic == TOPIC_EXT_NTP_REQUEST:
                    handle_ext_ntp(client, message)
                print(message.payload)


asyncio.run(main())
