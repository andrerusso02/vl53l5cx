#include "vl53l5cx_platform.h"
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <string.h>

#define VL53L5CX_MAX_I2C_CHUNK_SIZE 1024 

uint8_t WrMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, 
                uint8_t *p_values, uint32_t size)
{
    uint8_t addr_buf[2];
    struct i2c_msg msgs[2];
    uint32_t bytes_remaining = size;
    uint32_t current_offset = 0;

    while (bytes_remaining > 0) {
        uint16_t current_chunk_size = (bytes_remaining > VL53L5CX_MAX_I2C_CHUNK_SIZE) ? 
                                      VL53L5CX_MAX_I2C_CHUNK_SIZE : bytes_remaining;

        uint16_t current_reg_addr = RegisterAdress + current_offset;
        
        sys_put_be16(current_reg_addr, addr_buf);

        msgs[0].buf = addr_buf;
        msgs[0].len = 2U;
        msgs[0].flags = I2C_MSG_WRITE;

        msgs[1].buf = &p_values[current_offset];
        msgs[1].len = current_chunk_size;
        msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        int ret = i2c_transfer(p_platform->i2c->bus, msgs, 2, p_platform->address);
        if (ret != 0) {
            return 1;
        }

        bytes_remaining -= current_chunk_size;
        current_offset += current_chunk_size;
    }

    return 0;
}

uint8_t RdMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, 
                uint8_t *p_values, uint32_t size)
{
    uint8_t addr_buf[2];
    sys_put_be16(RegisterAdress, addr_buf);

    if (i2c_write_read(p_platform->i2c->bus, p_platform->address, 
                       addr_buf, 2, p_values, size) != 0) {
        return 1;
    }

    return 0;
}

uint8_t WrByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t value)
{
    return WrMulti(p_platform, RegisterAdress, &value, 1);
}

uint8_t RdByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_value)
{
    return RdMulti(p_platform, RegisterAdress, p_value, 1);
}

void SwapBuffer(uint8_t *buffer, uint16_t size)
{
    for (uint32_t i = 0; i < size; i += 4) {
        uint32_t tmp = sys_get_be32(&buffer[i]);
        sys_put_le32(tmp, &buffer[i]);
    }
}

uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs)
{
    k_msleep(TimeMs);
    return 0;
}

