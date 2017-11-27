/*                       RTV Real-Time Hypervisor
 * Copyright (C) 2017  Ying Ye
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "acpi.h"
#include "asm/asm-string.h"
#include "utils/screen.h"

#define MAX_CPU_COUNT 100
uint8_t g_acpiCpuCount;
uint8_t g_acpiCpuIds[MAX_CPU_COUNT];

static ACPI_MADT *s_madt;

static void AcpiParseApic(ACPI_MADT *madt)
{
  s_madt = madt;

  //g_localApicAddr = (uint8_t *)madt->localApicAddr;

  uint8_t *p = (uint8_t *) (madt + 1);
  uint8_t *end = (uint8_t *) madt + madt->header.length;

  while (p < end) {
    APIC_header *header = (APIC_header *) p;
    uint8_t type = header->type;
    uint8_t length = header->length;

    if (type == APIC_TYPE_LAPIC) {
        APIC_LAPIC *s = (APIC_LAPIC *) p;

        printf("Found CPU: %d %d %x\n", s->processor_id, s->apic_id, s->flags);
        if (g_acpiCpuCount < MAX_CPU_COUNT)
        {
            g_acpiCpuIds[g_acpiCpuCount] = s->apic_id;
            ++g_acpiCpuCount;
        }
    }
    else if (type == APIC_TYPE_IOAPIC) {
        APIC_IOAPIC *s = (APIC_IOAPIC *) p;

        printf("Found I/O APIC: %d 0x%08x %d\n", s->id, s->address, s->GSI_base);
        //g_ioApicAddr = (uint8_t *)s->ioApicAddress;
    }
    else if (type == APIC_TYPE_INTERRUPT_OVERRIDE) {
        APIC_InterruptOverride *s = (APIC_InterruptOverride *) p;

        printf("Found Interrupt Override: %d %d %d 0x%04x\n", s->bus, s->source, s->interrupt, s->flags);
    }
    else {
        printf("Unknown APIC structure %d\n", type);
    }

    p += length;
  }
}

static void AcpiParseDT(ACPI_header *header)
{
    uint32_t signature = header->signature;

    char sigStr[5];
    memcpy(sigStr, &signature, 4);
    sigStr[4] = 0;
    printf("%s 0x%x\n", sigStr, signature);

    if (signature == 0x43495041) {
        AcpiParseApic((ACPI_MADT *) header);
    }
}

static void AcpiParseRsdt(ACPI_header *rsdt)
{
    uint32_t *p = (uint32_t *)(rsdt + 1);
    uint32_t *end = (uint32_t *)((uint8_t*)rsdt + rsdt->length);

    while (p < end)
    {
        uint32_t address = *p++;
        printf("SDT addr: %X\n", address);
        AcpiParseDT((ACPI_header *) address);
    }
}

static void AcpiParseXsdt(ACPI_header *xsdt)
{
    uint64_t *p = (uint64_t *)(xsdt + 1);
    uint64_t *end = (uint64_t *)((uint8_t*)xsdt + xsdt->length);

    while (p < end)
    {
        uint64_t address = *p++;
        AcpiParseDT((ACPI_header *) address);
    }
}

static bool AcpiParseRsdp(uint8_t *p)
{
    // Parse Root System Description Pointer
    printf("RSDP found\n");

    // Verify checksum
    uint8_t sum = 0;
    for (uint8_t i = 0; i < 20; ++i)
    {
        sum += p[i];
    }

    if (sum)
    {
        printf("Checksum failed\n");
        return false;
    }
    // Print OEM
    char oem[7];
    memcpy(oem, p + 9, 6);
    oem[6] = '\0';
    printf("OEM = %s\n", oem);

    // Check version
    uint8_t revision = p[15];
    if (revision == 0)
    {
        printf("Version 1\n");

        uint32_t rsdtAddr = *(uint32_t *)(p + 16);
        AcpiParseRsdt((ACPI_header *)rsdtAddr);
    }
    else if (revision == 2)
    {
        printf("Version 2\n");

        uint32_t rsdtAddr = *(uint32_t *)(p + 16);
        uint64_t xsdtAddr = *(uint64_t *)(p + 24);

        if (xsdtAddr)
        {
            AcpiParseXsdt((ACPI_header *) xsdtAddr);
        }
        else
        {
            AcpiParseRsdt((ACPI_header *) rsdtAddr);
        }
    }
    else
    {
        printf("Unsupported ACPI version %d\n", revision);
    }

    return true;
}

void ACPI_init(void)
{
  // TODO - Search Extended BIOS Area

  /* Search main BIOS area below 1MB */
  uint8_t *p = (uint8_t *) 0x000e0000;
  uint8_t *end = (uint8_t *) 0x000fffff;

  while (p < end) {
    uint64_t signature = *(uint64_t *)p;

    /* "RSD PTR" */
    if (signature == 0x2052545020445352) {
      if (AcpiParseRsdp(p)) 
          break;
    }

    p += 16;
  }

  printf("Can't find RSDP\n");
}

uint8_t AcpiRemapIrq(uint8_t irq)
{
    ACPI_MADT *madt = s_madt;

    uint8_t *p = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (p < end)
    {
        APIC_header *header = (APIC_header *)p;
        uint8_t type = header->type;
        uint8_t length = header->length;

        if (type == APIC_TYPE_INTERRUPT_OVERRIDE)
        {
            APIC_InterruptOverride *s = (APIC_InterruptOverride *)p;

            if (s->source == irq)
            {
                return s->interrupt;
            }
        }

        p += length;
    }

    return irq;
}
