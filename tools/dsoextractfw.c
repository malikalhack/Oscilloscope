/**
 * @file    dsoextractfw.c
 * @version 0.2.2
 * @authors Oleg Khudyakov, Anton Chernov
 * @date    2026-09-04
 * @date    @showdate "%Y-%m-%d"
 * @brief Extracts Hantek FX2 firmware from an installed Windows driver
 * @par
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/******************************* Included files *******************************/
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <bfd.h>

/********************************* Definitions ********************************/

#define RECORD_SIZE             22U
#define MAX_DATA_SIZE           16U
#define RECORD_COUNT_OFFSET     0U
#define RECORD_ADDRESS_OFFSET   2U
#define RECORD_TYPE_OFFSET      4U
#define RECORD_DATA_OFFSET      5U
#define RECORD_TYPE_DATA        0U
#define RECORD_TYPE_EOF         1U

typedef struct {
    bfd_size_type firmwareOffset;
    bfd_size_type loaderOffset;
} SFirmwareOffsets_t;

/***************************** Private prototypes *****************************/

static int extractFirmware(const char *driverPath, const char *outputDir);
static bfd* openDriver(const char *driverPath);
static int findFirmwareOffsets(
    bfd *driver,
    asection *dataSection,
    SFirmwareOffsets_t *offsets
);
static int normalizeSymbolOffset(
    const asymbol *symbol,
    const asection *section,
    bfd_size_type *offset
);
static int writeHexFile(
    const char *path,
    const uint8_t *records,
    bfd_size_type length
);
static int writeHexRecord(FILE *output, const uint8_t *record);
static int isEmptyRecord(const uint8_t *record);
static int buildOutputPath(
    char *path,
    size_t pathLength,
    const char *outputDir,
    const char *driverPath,
    const char *suffix
);
static size_t getModelNameLength(const char *baseName, size_t stemLength);

/********************* Application Programming Interface *********************/

/** @fn main */
int main(int argc, char **argv) {
    const char *outputDir = ".";
    int result = EXIT_FAILURE;

    if ((argc < 2) || (argc > 3)) {
        fprintf(stderr, "Usage: %s DRIVER.SYS [OUTPUT_DIR]\n", argv[0]);
    }
    else {
        if (argc == 3) {
            outputDir = argv[2];
        }
        result = extractFirmware(argv[1], outputDir);
    }

    return result;
}

/****************************** Private functions *****************************/

/** @fn extractFirmware */
static int extractFirmware(const char *driverPath, const char *outputDir) {
    bfd *driver = NULL;
    asection *dataSection = NULL;
    uint8_t *data = NULL;
    SFirmwareOffsets_t offsets = {0U, 0U};
    bfd_size_type dataLength = 0U;
    bfd_size_type firmwareLength = 0U;
    bfd_size_type loaderLength = 0U;
    char firmwarePath[1024];
    char loaderPath[1024];
    int result = EXIT_FAILURE;

    bfd_init();
    driver = openDriver(driverPath);
    if (driver == NULL) {
        fprintf(stderr, "Unable to open driver as a PE/COFF object\n");
    }
    else {
        dataSection = bfd_get_section_by_name(driver, ".data");
        if (dataSection == NULL) {
            fprintf(stderr, "Driver does not contain a .data section\n");
        }
        else if (findFirmwareOffsets(driver, dataSection, &offsets) != 0) {
            fprintf(stderr, "Driver does not contain firmware symbols\n");
        }
        else {
            dataLength = bfd_section_size(dataSection);
            firmwareLength = offsets.loaderOffset - offsets.firmwareOffset;
            loaderLength = dataLength - offsets.loaderOffset;
            data = (uint8_t*)malloc(dataLength);

            if (data == NULL) {
                perror("Unable to allocate .data buffer");
            }
            else if (!bfd_get_section_contents(
                driver, dataSection, data, 0U, dataLength
            )) {
                bfd_perror("Unable to read .data section");
            }
            else if (
                (buildOutputPath(
                    firmwarePath,
                    sizeof(firmwarePath),
                    outputDir,
                    driverPath,
                    "_firmware.hex"
                ) != 0) ||
                (buildOutputPath(
                    loaderPath,
                    sizeof(loaderPath),
                    outputDir,
                    driverPath,
                    "_loader.hex"
                ) != 0)
            ) {
                fprintf(stderr, "Output path is too long\n");
            }
            else if (
                writeHexFile(
                    firmwarePath,
                    &data[offsets.firmwareOffset],
                    firmwareLength
                ) != 0
            ) {
                fprintf(stderr, "Unable to write %s\n", firmwarePath);
            }
            else if (
                writeHexFile(
                    loaderPath,
                    &data[offsets.loaderOffset],
                    loaderLength
                ) != 0
            ) {
                fprintf(stderr, "Unable to write %s\n", loaderPath);
            }
            else {
                printf("Wrote %s and %s\n", firmwarePath, loaderPath);
                result = EXIT_SUCCESS;
            }
        }
    }

    free(data);
    if (driver != NULL) {
        bfd_close(driver);
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn openDriver */
static bfd* openDriver(const char *driverPath) {
    static const char * const targets[] = {
        NULL, "pei-i386", "efi-app-ia32"
    };
    bfd *driver = NULL;
    size_t index = 0U;

    for (
        index = 0U;
        (index < (sizeof(targets) / sizeof(targets[0]))) && (driver == NULL);
        ++index
    ) {
        driver = bfd_openr(driverPath, targets[index]);
        if (driver != NULL) {
            if (!bfd_check_format(driver, bfd_object)) {
                bfd_close(driver);
                driver = NULL;
            }
        }
    }

    return driver;
}

/*----------------------------------------------------------------------------*/

/** @fn findFirmwareOffsets */
static int findFirmwareOffsets(
    bfd *driver,
    asection *dataSection,
    SFirmwareOffsets_t *offsets
) {
    asymbol **symbols = NULL;
    long symbolStorage = 0L;
    long symbolCount = 0L;
    long index = 0L;
    int firmwareFound = 0;
    int loaderFound = 0;
    int result = -1;

    symbolStorage = bfd_get_symtab_upper_bound(driver);
    if (symbolStorage > 0L) {
        symbols = (asymbol**)malloc((size_t)symbolStorage);
    }

    if (symbols != NULL) {
        symbolCount = bfd_canonicalize_symtab(driver, symbols);
        for (index = 0L; index < symbolCount; ++index) {
            if (strcmp(bfd_asymbol_name(symbols[index]), "_firmware") == 0) {
                firmwareFound = normalizeSymbolOffset(
                    symbols[index], dataSection, &offsets->firmwareOffset
                ) == 0;
            }
            else if (strcmp(bfd_asymbol_name(symbols[index]), "_loader") == 0) {
                loaderFound = normalizeSymbolOffset(
                    symbols[index], dataSection, &offsets->loaderOffset
                ) == 0;
            }
            else {
                /* Other driver symbols are not relevant. */
            }
        }
    }

    if (
        (firmwareFound != 0) &&
        (loaderFound != 0) &&
        (offsets->firmwareOffset < offsets->loaderOffset) &&
        (offsets->loaderOffset < bfd_section_size(dataSection))
    ) {
        result = 0;
    }

    free(symbols);
    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn normalizeSymbolOffset */
static int normalizeSymbolOffset(
    const asymbol *symbol,
    const asection *section,
    bfd_size_type *offset
) {
    const bfd_vma value = symbol->value;
    const bfd_size_type sectionLength = bfd_section_size(section);
    const file_ptr fileOffset = section->filepos;
    int result = -1;

    if (
        (value >= (bfd_vma)fileOffset) &&
        ((value - (bfd_vma)fileOffset) < sectionLength)
    ) {
        *offset = (bfd_size_type)(value - (bfd_vma)fileOffset);
        result = 0;
    }
    else if (value < sectionLength) {
        *offset = (bfd_size_type)value;
        result = 0;
    }
    else {
        /* Unsupported symbol encoding. */
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn writeHexFile */
static int writeHexFile(
    const char *path,
    const uint8_t *records,
    const bfd_size_type length
) {
    FILE *output = NULL;
    bfd_size_type offset = 0U;
    int eofFound = 0;
    int result = -1;

    output = fopen(path, "w");
    if (output == NULL) {
        perror(path);
    }
    else if ((length == 0U) || ((length % RECORD_SIZE) != 0U)) {
        fprintf(stderr, "%s: invalid record array length\n", path);
    }
    else {
        result = 0;
        for (
            offset = 0U;
            (offset < length) && (eofFound == 0) && (result == 0);
            offset += RECORD_SIZE
        ) {
            if (isEmptyRecord(&records[offset]) == 0) {
                result = writeHexRecord(output, &records[offset]);
                if (
                    (records[offset + RECORD_COUNT_OFFSET] == 0U) &&
                    (records[offset + RECORD_TYPE_OFFSET] == RECORD_TYPE_EOF)
                ) {
                    eofFound = 1;
                }
            }
        }

        if (eofFound == 0) {
            fprintf(stderr, "%s: Intel HEX EOF record not found\n", path);
            result = -1;
        }
    }

    if (output != NULL) {
        if (fclose(output) != 0) {
            result = -1;
        }
        if (result != 0) {
            remove(path);
        }
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn writeHexRecord */
static int writeHexRecord(FILE *output, const uint8_t *record) {
    const uint8_t count = record[RECORD_COUNT_OFFSET];
    const uint16_t address =
        (uint16_t)record[RECORD_ADDRESS_OFFSET] |
        ((uint16_t)record[RECORD_ADDRESS_OFFSET + 1U] << 8U);
    const uint8_t type = record[RECORD_TYPE_OFFSET];
    uint8_t checksum = count;
    uint8_t index = 0U;
    int result = -1;

    if (
        (count <= MAX_DATA_SIZE) &&
        ((type == RECORD_TYPE_DATA) || (type == RECORD_TYPE_EOF)) &&
        !((type == RECORD_TYPE_EOF) && ((count != 0U) || (address != 0U)))
    ) {
        checksum = (uint8_t)(checksum + (uint8_t)(address >> 8U));
        checksum = (uint8_t)(checksum + (uint8_t)address);
        checksum = (uint8_t)(checksum + type);
        fprintf(output, ":%02X%04X%02X", count, address, type);

        for (index = 0U; index < count; ++index) {
            checksum = (uint8_t)(
                checksum + record[RECORD_DATA_OFFSET + index]
            );
            fprintf(output, "%02X", record[RECORD_DATA_OFFSET + index]);
        }

        checksum = (uint8_t)(0U - checksum);
        fprintf(output, "%02X\n", checksum);
        result = ferror(output) == 0 ? 0 : -1;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn isEmptyRecord */
static int isEmptyRecord(const uint8_t *record) {
    size_t index = 0U;
    int result = 1;

    for (index = 0U; (index < RECORD_SIZE) && (result != 0); ++index) {
        if (record[index] != 0U) {
            result = 0;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn buildOutputPath */
static int buildOutputPath(
    char *path,
    const size_t pathLength,
    const char *outputDir,
    const char *driverPath,
    const char *suffix
) {
    const char *baseName = strrchr(driverPath, '/');
    const char *extension = NULL;
    size_t modelLength = 0U;
    int written = 0;
    int result = -1;

    baseName = baseName == NULL ? driverPath : baseName + 1;
    extension = strrchr(baseName, '.');
    modelLength = extension == NULL ? strlen(baseName) :
        (size_t)(extension - baseName);
    modelLength = getModelNameLength(baseName, modelLength);

    if (
        (modelLength >= 3U) &&
        (strncasecmp(baseName, "DSO", 3U) == 0)
    ) {
        written = snprintf(
            path,
            pathLength,
            "%s/DSO%.*s%s",
            outputDir,
            (int)(modelLength - 3U),
            &baseName[3],
            suffix
        );
    }
    else {
        written = snprintf(
            path,
            pathLength,
            "%s/%.*s%s",
            outputDir,
            (int)modelLength,
            baseName,
            suffix
        );
    }
    if ((written >= 0) && ((size_t)written < pathLength)) {
        result = 0;
    }

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn getModelNameLength */
static size_t getModelNameLength(
    const char *baseName,
    const size_t stemLength
) {
    static const char * const architectures[] = {"x86", "AMD64", "IA64"};
    size_t modelLength = stemLength;
    size_t architectureLength = 0U;
    size_t index = 0U;

    if (
        (modelLength > 0U) &&
        ((baseName[modelLength - 1U] == '1') ||
            (baseName[modelLength - 1U] == '2'))
    ) {
        --modelLength;
    }

    for (index = 0U; index < 3U; ++index) {
        architectureLength = strlen(architectures[index]);
        if (
            (modelLength >= architectureLength) &&
            (strncasecmp(
                &baseName[modelLength - architectureLength],
                architectures[index],
                architectureLength
            ) == 0)
        ) {
            modelLength -= architectureLength;
        }
    }

    return modelLength;
}

/******************************************************************************/