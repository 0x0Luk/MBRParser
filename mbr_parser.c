#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_COUNT 4

typedef struct {
    uint8_t boot_flag;     
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t partition_type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;   
    uint32_t sectors_count;
} PartitionEntry;

int is_gpt(FILE *file) {
    uint8_t gpt_header[8];
    fseek(file, SECTOR_SIZE, SEEK_SET);
    fread(gpt_header, 1, 8, file);
    if (memcmp(gpt_header, "EFI PART", 8) == 0) {
        return 1; 
    }
    return 0; 
}

const char* get_partition_type(uint8_t type) {
    switch(type) {
        case 0x83: return "Linux";
        case 0x07: return "NTFS/exFAT";
        case 0x0B: return "FAT32";
        case 0x0C: return "FAT32 LBA";
        case 0x05: return "Extended";
        default: return "Desconhecido";
    }
}

void print_partition_info(PartitionEntry *entry, int partition_number) {
    printf("Partição %d:\n", partition_number);
    printf("  Status de Boot: %s\n", entry->boot_flag == 0x80 ? "Ativa (Bootável)" : "Inativa");
    printf("  Tipo de Partição: %s (0x%02X)\n", get_partition_type(entry->partition_type), entry->partition_type);
    printf("  Início (LBA): %u\n", entry->start_lba);
    printf("  Fim (LBA): %u\n", entry->start_lba + entry->sectors_count - 1);
    printf("  Número de Setores: %u\n", entry->sectors_count);
    printf("  Tamanho da Partição: %u MB\n", (entry->sectors_count * 512) / (1024 * 1024));
    printf("-----------------------------------\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Uso: %s <arquivo_mbr> [dispositivo]\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        return EXIT_FAILURE;
    }

    if (is_gpt(file)) {
        printf("Tipo de Disklabel: gpt\n");
    } else {
        printf("Tipo de Disklabel: dos\n");
    }

    fseek(file, 0, SEEK_SET);
    
    uint8_t mbr[SECTOR_SIZE];
    if (fread(mbr, 1, SECTOR_SIZE, file) != SECTOR_SIZE) {
        fprintf(stderr, "Erro ao ler o arquivo MBR\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    fclose(file);

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        fprintf(stderr, "Assinatura de MBR inválida\n");
        return EXIT_FAILURE;
    }

    uint32_t disk_identifier = *((uint32_t*)&mbr[440]);
    printf("Identificador do Disco: 0x%08x\n", disk_identifier);

    printf("\n--- Tabela de Partições ---\n");

    PartitionEntry partitions[PARTITION_COUNT];
    for (int i = 0; i < PARTITION_COUNT; ++i) {
        int offset = PARTITION_TABLE_OFFSET + i * PARTITION_ENTRY_SIZE;
        partitions[i].boot_flag = mbr[offset];
        partitions[i].start_head = mbr[offset + 1];
        partitions[i].start_sector = mbr[offset + 2] & 0x3F;
        partitions[i].start_cylinder = ((mbr[offset + 2] & 0xC0) << 2) | mbr[offset + 3];
        partitions[i].partition_type = mbr[offset + 4];
        partitions[i].end_head = mbr[offset + 5];
        partitions[i].end_sector = mbr[offset + 6] & 0x3F;
        partitions[i].end_cylinder = ((mbr[offset + 6] & 0xC0) << 2) | mbr[offset + 7];
        partitions[i].start_lba = *((uint32_t *)&mbr[offset + 8]);
        partitions[i].sectors_count = *((uint32_t *)&mbr[offset + 12]);

        if (partitions[i].partition_type != 0x00) {
            print_partition_info(&partitions[i], i + 1);
        }
    }

    return EXIT_SUCCESS;
}
