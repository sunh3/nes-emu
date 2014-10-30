#include "Ppu.h"
#include "Memory.h"
#include "Renderer.h"
#include "Bitfield.h"

Ppu::Ppu()
	: m_cpuRam(nullptr)
	, m_ppuRam(nullptr)
	, m_spriteRam(nullptr)
{
}

void Ppu::Initialize(CpuRam& cpuRam, PpuRam& ppuRam, SpriteRam& spriteRam)
{
	m_cpuRam = &cpuRam;
	m_ppuRam = &ppuRam;
	m_spriteRam = &spriteRam;

	m_ppuControl1 = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControl1);
	m_ppuControl2 = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuControl2);
	m_ppuStatus = m_cpuRam->UnsafePtrAs<Bitfield8>(CpuRam::kPpuStatus);
}

void Ppu::Reset()
{
	// Fake a first vblank
	m_ppuStatus->Set(PpuStatus::InVBlank);
}

void Ppu::Run()
{
	printf("Ppu cycle\n");
}

void Ppu::OnCpuMemoryRead(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuStatus: // $2002
		m_ppuStatus->Clear(PpuStatus::InVBlank);
		//@TODO: After a read occurs, $2005 is reset, hence the next write to $2005 will be Horizontal.

		// After a read occurs, $2006 is reset, hence the next write to $2006 will be the high byte portion.
		m_vramAddress.high = true;

		break;
	}
}

void Ppu::OnCpuMemoryWrite(uint16 address)
{
	switch (address)
	{
	case CpuRam::kPpuVRamAddress2: // $2006
		{
			// Implement double write (alternate between writing high byte and low byte)
			uint16 value = TO16(m_cpuRam->Read8(address));
			if (m_vramAddress.high)
				m_vramAddress.address = (value << 8) | (m_vramAddress.address & 0x00FF);
			else
				m_vramAddress.address = value | (m_vramAddress.address & 0xFF00);
			
			m_vramAddress.high = !m_vramAddress.high;
		}
		break;

	case CpuRam::kPpuVRamIO: // $2007
		{
			// Write value to VRAM and increment address
			uint8 value = m_cpuRam->Read8(address);
			m_ppuRam->Write8(m_vramAddress.address, value);
			m_vramAddress.address += PpuControl1::GetPpuAddressIncrementSize( m_ppuControl1->Value() );
		}
		break;
	}
}

void Ppu::HACK_DrawPatternTables()
{
	Renderer renderer;
	renderer.Create();

	const uint8* chrRom = m_ppuRam->UnsafePtr(0);

	static auto ReadTile = [&chrRom] (int tileIndex, uint8 tile[8][8])
	{
		// Every 16 bytes is 1 8x8 tile
		const int tileOffset = tileIndex * 16;
		assert(tileOffset+16<KB(16));
		Bitfield8* pByte1 = (Bitfield8*)&chrRom[tileOffset + 0];
		Bitfield8* pByte2 = (Bitfield8*)&chrRom[tileOffset + 8];

		for (size_t y = 0; y < 8; ++y)
		{
			tile[0][y] = pByte1->TestPos(7) | (pByte2->TestPos(7) << 1);
			tile[1][y] = pByte1->TestPos(6) | (pByte2->TestPos(6) << 1);
			tile[2][y] = pByte1->TestPos(5) | (pByte2->TestPos(5) << 1);
			tile[3][y] = pByte1->TestPos(4) | (pByte2->TestPos(4) << 1);
			tile[4][y] = pByte1->TestPos(3) | (pByte2->TestPos(3) << 1);
			tile[5][y] = pByte1->TestPos(2) | (pByte2->TestPos(2) << 1);
			tile[6][y] = pByte1->TestPos(1) | (pByte2->TestPos(1) << 1);
			tile[7][y] = pByte1->TestPos(0) | (pByte2->TestPos(0) << 1);

			++pByte1;
			++pByte2;
		}
	};

	static auto DrawTile = [&renderer] (int x, int y, uint8 tile[8][8])
	{
		Color4 color;

		for (size_t ty = 0; ty < 8; ++ty)
		{
			for (size_t tx = 0; tx < 8; ++tx)
			{
				switch (tile[tx][ty])
				{
				case 0: color = Color4::Black(); break;
				case 1: color = Color4::Blue(); break;
				case 2: color = Color4::Red(); break;
				case 3: color = Color4::Green(); break;
				default: assert(false); break;
				}

				renderer.DrawPixel(x + tx, y + ty, color);
			}
		}
	};			

	for ( ; ; )
	{
		renderer.Clear();

		// Render Pattern Table #0: [$0000,$1000[ (4 kb) = 256 tiles
		// Render Pattern Table #1: [$1000,$2000[ (4 kb) = 256 tiles

		int tileIndex = 0;
		uint8 tile[8][8] = {0};
		for (int y = 0; y < 32; ++y)
		{
			for (int x = 0; x < 16; ++x)
			{
				ReadTile(tileIndex, tile);
				DrawTile(x*8, y*8, tile);
				++tileIndex;
			}
		}

		renderer.Render();
	}
}