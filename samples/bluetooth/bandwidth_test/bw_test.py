import asyncio, tqdm, uuid
import itertools
from asyncio import sleep
from bleak import BleakScanner, BleakClient
from bleak.backends.scanner import AdvertisementData
from bleak.backends.device import BLEDevice

BWT_UUID = uuid.UUID('5432e12b-34be-4996-aafa-bdc0c7554b89')

def handle_disconnect(_: BleakClient):
	print("Device was disconnected, goodbye.")
	# cancelling all tasks effectively ends the program
	for task in asyncio.all_tasks():
		task.cancel()

def match_name(device: BLEDevice, adv: AdvertisementData):
	if device.name:
		print(f"Device: {device.name}")
	return "T-Mobile" in (device.name or "")

async def main():
	device = None
	print("Scanning for devedge board")
	while not device:
		device = await BleakScanner.find_device_by_filter(match_name)
		if not device:
			await sleep(5)
	async with BleakClient(device, disconnected_callback=handle_disconnect) as client:
		await sleep(2)
		services = await client.get_services()
		chrcs = itertools.chain(*(i.characteristics for i in services))
		chrc = None
		for i in chrcs:
			if uuid.UUID(i.uuid) == BWT_UUID:
				chrc = i
		if not i:
			print("Error: could not find characteristic")
			exit()
		print("Testing sending 1MB")
		remaining = 1000000
		bar = tqdm.tqdm(total=remaining, unit="B")
		while remaining:
			sz = min(228, remaining)
			remaining -= sz
			await client.write_gatt_char(chrc, bytes(sz), True)
			_ = bar.update(sz)
		bar.close()
try:
	asyncio.run(main())
except asyncio.CancelledError:
	# task is cancelled on disconnect, so we ignore this error
	pass
