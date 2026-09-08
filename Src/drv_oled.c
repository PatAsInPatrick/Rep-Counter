/*******************************************************************************
 * File Name    : drv_oled.c
 * Description  : SSD1306 driver. The font holds ASCII 32 to 90, which covers
 *                digits, capital letters and the few symbols used here.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "drv_oled.h"
#include "drv_i2c.h"

/* Private define ------------------------------------------------------------*/
#define OLED_ADDRESS                (0x3Cu) /* use 0x3D if the module says 7A */
#define OLED_CONTROL_COMMAND        (0x00u)
#define OLED_CONTROL_DATA           (0x40u)

#define OLED_PAGE_COUNT             (8u)
#define OLED_PAGE_SIZE              (128u)  /* bytes of one page             */
#define OLED_BUFFER_SIZE            (1024u) /* 128 x 64 / 8                  */

#define OLED_CMD_PAGE_BASE          (0xB0u) /* select page 0 to 7            */
/* Some panels hide the first columns behind the glass. Raise this value to
 * 2 if the left edge of the text is still cut off after the layout margin. */
#define OLED_COLUMN_OFFSET          (0u)
#define OLED_CMD_COLUMN_LOW         (uint8_t)(0x00u | (OLED_COLUMN_OFFSET & 0x0Fu))
#define OLED_CMD_COLUMN_HIGH        (uint8_t)(0x10u | (OLED_COLUMN_OFFSET >> 4u))
#define OLED_CMD_FRAME_LENGTH       (3u)
#define OLED_BOOT_DELAY_LOOP        (200000uL)

#define OLED_FONT_FIRST_CHAR        (32u)
#define OLED_FONT_LAST_CHAR         (90u)
#define OLED_FONT_WIDTH             (5u)
#define OLED_FONT_HEIGHT            (7u)
#define OLED_CHAR_ADVANCE           (6u)

#define OLED_PIXEL_PER_BYTE         (8u)
#define OLED_PERCENT_FULL           (100u)
#define OLED_DECIMAL_BASE           (10u)
#define OLED_HUNDRED                (100u)
#define OLED_MS_PER_HUNDREDTH       (10u)
#define OLED_DIGIT_MAX              (5u)

/* Private variables ---------------------------------------------------------*/
static uint8_t u1g_oledBuffer[OLED_BUFFER_SIZE] = {0u};

static const uint8_t u1g_oledFont[(OLED_FONT_LAST_CHAR - OLED_FONT_FIRST_CHAR + 1u)
                                  * OLED_FONT_WIDTH] =
{
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* space */
    0x00u, 0x00u, 0x5Fu, 0x00u, 0x00u,   /* ! */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* " */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* # */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* $ */
    0x63u, 0x13u, 0x08u, 0x64u, 0x63u,   /* % */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* & */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* ' */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* ( */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* ) */
    0x2Au, 0x1Cu, 0x3Eu, 0x1Cu, 0x2Au,   /* * */
    0x08u, 0x08u, 0x3Eu, 0x08u, 0x08u,   /* + */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* , */
    0x08u, 0x08u, 0x08u, 0x08u, 0x08u,   /* - */
    0x00u, 0x60u, 0x60u, 0x00u, 0x00u,   /* . */
    0x60u, 0x10u, 0x08u, 0x04u, 0x03u,   /* / */
    0x3Eu, 0x51u, 0x49u, 0x45u, 0x3Eu,   /* 0 */
    0x00u, 0x42u, 0x7Fu, 0x40u, 0x00u,   /* 1 */
    0x42u, 0x61u, 0x51u, 0x49u, 0x46u,   /* 2 */
    0x21u, 0x41u, 0x45u, 0x4Bu, 0x31u,   /* 3 */
    0x18u, 0x14u, 0x12u, 0x7Fu, 0x10u,   /* 4 */
    0x27u, 0x45u, 0x45u, 0x45u, 0x39u,   /* 5 */
    0x3Cu, 0x4Au, 0x49u, 0x49u, 0x30u,   /* 6 */
    0x01u, 0x71u, 0x09u, 0x05u, 0x03u,   /* 7 */
    0x36u, 0x49u, 0x49u, 0x49u, 0x36u,   /* 8 */
    0x06u, 0x49u, 0x49u, 0x29u, 0x1Eu,   /* 9 */
    0x00u, 0x00u, 0x36u, 0x00u, 0x00u,   /* : */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* ; */
    0x08u, 0x14u, 0x22u, 0x41u, 0x00u,   /* < */
    0x14u, 0x14u, 0x14u, 0x14u, 0x14u,   /* = */
    0x00u, 0x41u, 0x22u, 0x14u, 0x08u,   /* > */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* ? */
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,   /* @ */
    0x7Eu, 0x09u, 0x09u, 0x09u, 0x7Eu,   /* A */
    0x7Fu, 0x49u, 0x49u, 0x49u, 0x36u,   /* B */
    0x3Eu, 0x41u, 0x41u, 0x41u, 0x22u,   /* C */
    0x7Fu, 0x41u, 0x41u, 0x22u, 0x1Cu,   /* D */
    0x7Fu, 0x49u, 0x49u, 0x49u, 0x41u,   /* E */
    0x7Fu, 0x09u, 0x09u, 0x09u, 0x01u,   /* F */
    0x3Eu, 0x41u, 0x49u, 0x49u, 0x7Au,   /* G */
    0x7Fu, 0x08u, 0x08u, 0x08u, 0x7Fu,   /* H */
    0x00u, 0x41u, 0x7Fu, 0x41u, 0x00u,   /* I */
    0x20u, 0x40u, 0x41u, 0x3Fu, 0x01u,   /* J */
    0x7Fu, 0x08u, 0x14u, 0x22u, 0x41u,   /* K */
    0x7Fu, 0x40u, 0x40u, 0x40u, 0x40u,   /* L */
    0x7Fu, 0x02u, 0x0Cu, 0x02u, 0x7Fu,   /* M */
    0x7Fu, 0x02u, 0x04u, 0x08u, 0x7Fu,   /* N */
    0x3Eu, 0x41u, 0x41u, 0x41u, 0x3Eu,   /* O */
    0x7Fu, 0x09u, 0x09u, 0x09u, 0x06u,   /* P */
    0x3Eu, 0x41u, 0x51u, 0x21u, 0x5Eu,   /* Q */
    0x7Fu, 0x09u, 0x19u, 0x29u, 0x46u,   /* R */
    0x46u, 0x49u, 0x49u, 0x49u, 0x31u,   /* S */
    0x01u, 0x01u, 0x7Fu, 0x01u, 0x01u,   /* T */
    0x3Fu, 0x40u, 0x40u, 0x40u, 0x3Fu,   /* U */
    0x1Fu, 0x20u, 0x40u, 0x20u, 0x1Fu,   /* V */
    0x7Fu, 0x20u, 0x18u, 0x20u, 0x7Fu,   /* W */
    0x63u, 0x14u, 0x08u, 0x14u, 0x63u,   /* X */
    0x03u, 0x04u, 0x78u, 0x04u, 0x03u,   /* Y */
    0x61u, 0x51u, 0x49u, 0x45u, 0x43u,   /* Z */
};

static const uint8_t u1g_oledInitSequence[] =
{
    0xAEu,                  /* display off                                   */
    0xD5u, 0x80u,           /* clock divide ratio                            */
    0xA8u, 0x3Fu,           /* multiplex ratio = 64 rows                     */
    0xD3u, 0x00u,           /* display offset                                */
    0x40u,                  /* start line = 0                                */
    0x8Du, 0x14u,           /* charge pump on                                */
    0x20u, 0x02u,           /* page addressing mode                          */
    0xA1u,                  /* segment remap                                 */
    0xC8u,                  /* scan direction reversed                       */
    0xDAu, 0x12u,           /* com pin configuration                         */
    0x81u, 0x7Fu,           /* contrast                                      */
    0xD9u, 0xF1u,           /* pre charge period                             */
    0xDBu, 0x40u,           /* vcom deselect level                           */
    0xA4u,                  /* follow the RAM content                        */
    0xA6u,                  /* normal, not inverted                          */
    0xAFu                   /* display on                                    */
};

/* Private function prototypes -----------------------------------------------*/
static void v_oledSendCommandList(const uint8_t * pu1_list, uint16_t u2_length);
static void v_oledSetPixel(uint8_t u1_x, uint8_t u1_y);
static void v_oledDrawChar(uint8_t u1_x, uint8_t u1_y, char c_char,
                           uint8_t u1_scale);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_oledSendCommandList
 * @brief             - Send every configuration byte inside a single frame.
 *                      One control byte of 0x00 tells the display that all
 *                      the bytes that follow are commands.
 * @param[in]         - pu1_list  : command bytes
 * @param[in]         - u2_length : number of bytes
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_oledSendCommandList(const uint8_t * pu1_list, uint16_t u2_length)
{
    (void)u1_i2cWriteBlock((uint8_t)OLED_ADDRESS, (uint8_t)OLED_CONTROL_COMMAND,
                           pu1_list, u2_length);
}

/*********************************************************************
 * @fn                - v_oledSetPixel
 * @brief             - Turn on one pixel inside the frame buffer.
 * @param[in]         - u1_x : column 0..127
 * @param[in]         - u1_y : row 0..63
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_oledSetPixel(uint8_t u1_x, uint8_t u1_y)
{
    uint16_t u2t_index = 0u;

    if ((u1_x < (uint8_t)OLED_WIDTH) && (u1_y < (uint8_t)OLED_HEIGHT))
    {
        u2t_index = (uint16_t)u1_x
                  + ((uint16_t)(u1_y / OLED_PIXEL_PER_BYTE) * OLED_WIDTH);
        u1g_oledBuffer[u2t_index] |= (uint8_t)(1u << (u1_y % OLED_PIXEL_PER_BYTE));
    }
}

/*********************************************************************
 * @fn                - v_oledDrawChar
 * @brief             - Draw one character, enlarged by an integer factor.
 * @param[in]         - u1_x     : left position in pixels
 * @param[in]         - u1_y     : top position in pixels
 * @param[in]         - c_char   : character to draw
 * @param[in]         - u1_scale : 1 = normal size
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_oledDrawChar(uint8_t u1_x, uint8_t u1_y, char c_char,
                           uint8_t u1_scale)
{
    uint8_t u1t_code = (uint8_t)c_char;
    uint8_t u1t_column = 0u;
    uint8_t u1t_row = 0u;
    uint8_t u1t_dx = 0u;
    uint8_t u1t_dy = 0u;
    uint8_t u1t_bits = 0u;
    uint16_t u2t_base = 0u;

    if ((u1t_code < (uint8_t)OLED_FONT_FIRST_CHAR)
        || (u1t_code > (uint8_t)OLED_FONT_LAST_CHAR))
    {
        u1t_code = (uint8_t)OLED_FONT_FIRST_CHAR;
    }

    u2t_base = (uint16_t)((uint16_t)(u1t_code - OLED_FONT_FIRST_CHAR)
             * OLED_FONT_WIDTH);

    for (u1t_column = 0u; u1t_column < (uint8_t)OLED_FONT_WIDTH; u1t_column++)
    {
        u1t_bits = u1g_oledFont[u2t_base + u1t_column];

        for (u1t_row = 0u; u1t_row < (uint8_t)OLED_FONT_HEIGHT; u1t_row++)
        {
            if ((u1t_bits & (uint8_t)(1u << u1t_row)) != 0u)
            {
                for (u1t_dx = 0u; u1t_dx < u1_scale; u1t_dx++)
                {
                    for (u1t_dy = 0u; u1t_dy < u1_scale; u1t_dy++)
                    {
                        v_oledSetPixel((uint8_t)(u1_x + (u1t_column * u1_scale) + u1t_dx),
                                       (uint8_t)(u1_y + (u1t_row * u1_scale) + u1t_dy));
                    }
                }
            }
        }
    }
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_oledInit
 * @brief             - Send the start-up sequence and clear the screen.
 *                      I2C must be initialised before calling this.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledInit(void)
{
    volatile uint32_t u4t_delay = 0uL;

    for (u4t_delay = 0uL; u4t_delay < OLED_BOOT_DELAY_LOOP; u4t_delay++)
    {
        /* let the panel supply settle before the first command */
    }

    v_oledSendCommandList(u1g_oledInitSequence,
                          (uint16_t)sizeof(u1g_oledInitSequence));

    for (u4t_delay = 0uL; u4t_delay < OLED_BOOT_DELAY_LOOP; u4t_delay++)
    {
        /* let the charge pump reach its working voltage */
    }

    v_oledClear();
    v_oledFlush();
}

/*********************************************************************
 * @fn                - v_oledClear
 * @brief             - Erase the frame buffer. Nothing is sent yet.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledClear(void)
{
    uint16_t u2t_index = 0u;

    for (u2t_index = 0u; u2t_index < (uint16_t)OLED_BUFFER_SIZE; u2t_index++)
    {
        u1g_oledBuffer[u2t_index] = 0u;
    }
}

/*********************************************************************
 * @fn                - v_oledDrawString
 * @brief             - Draw a text line into the frame buffer.
 * @param[in]         - u1_x     : left position in pixels
 * @param[in]         - u1_y     : top position in pixels
 * @param[in]         - pc_text  : text ending with a zero byte
 * @param[in]         - u1_scale : 1 = normal size
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledDrawString(uint8_t u1_x, uint8_t u1_y, const char * pc_text,
                      uint8_t u1_scale)
{
    uint8_t  u1t_x = u1_x;
    uint16_t u2t_index = 0u;

    if (pc_text != NULL)
    {
        while (pc_text[u2t_index] != '\0')
        {
            v_oledDrawChar(u1t_x, u1_y, pc_text[u2t_index], u1_scale);
            u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));
            u2t_index++;
        }
    }
}

/*********************************************************************
 * @fn                - v_oledDrawNumber
 * @brief             - Draw an unsigned number into the frame buffer.
 * @param[in]         - u1_x     : left position in pixels
 * @param[in]         - u1_y     : top position in pixels
 * @param[in]         - u2_value : value to draw
 * @param[in]         - u1_scale : 1 = normal size
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledDrawNumber(uint8_t u1_x, uint8_t u1_y, uint16_t u2_value,
                      uint8_t u1_scale)
{
    char     c_digit[OLED_DIGIT_MAX];
    uint8_t  u1t_count = 0u;
    uint8_t  u1t_x = u1_x;
    uint16_t u2t_work = u2_value;

    do
    {
        c_digit[u1t_count] = (char)((u2t_work % OLED_DECIMAL_BASE) + (uint16_t)'0');
        u2t_work = u2t_work / OLED_DECIMAL_BASE;
        u1t_count++;
    }
    while ((u2t_work > 0u) && (u1t_count < (uint8_t)OLED_DIGIT_MAX));

    while (u1t_count > 0u)
    {
        u1t_count--;
        v_oledDrawChar(u1t_x, u1_y, c_digit[u1t_count], u1_scale);
        u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));
    }
}

/*********************************************************************
 * @fn                - v_oledDrawSeconds
 * @brief             - Draw a millisecond value as seconds with two
 *                      decimals, for example 480 becomes 0.48
 * @param[in]         - u1_x     : left position in pixels
 * @param[in]         - u1_y     : top position in pixels
 * @param[in]         - u2_milli : value in milliseconds
 * @param[in]         - u1_scale : 1 = normal size
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledDrawSeconds(uint8_t u1_x, uint8_t u1_y, uint16_t u2_milli,
                       uint8_t u1_scale)
{
    uint16_t u2t_hundredth = (uint16_t)(u2_milli / OLED_MS_PER_HUNDREDTH);
    uint16_t u2t_whole = (uint16_t)(u2t_hundredth / OLED_HUNDRED);
    uint16_t u2t_fraction = (uint16_t)(u2t_hundredth % OLED_HUNDRED);
    uint8_t  u1t_x = u1_x;

    v_oledDrawNumber(u1t_x, u1_y, u2t_whole, u1_scale);
    if (u2t_whole >= (uint16_t)OLED_DECIMAL_BASE)
    {
        u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));
    }
    u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));

    v_oledDrawString(u1t_x, u1_y, ".", u1_scale);
    u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));

    if (u2t_fraction < (uint16_t)OLED_DECIMAL_BASE)
    {
        v_oledDrawString(u1t_x, u1_y, "0", u1_scale);
        u1t_x = (uint8_t)(u1t_x + (OLED_CHAR_ADVANCE * u1_scale));
    }
    v_oledDrawNumber(u1t_x, u1_y, u2t_fraction, u1_scale);
}

/*********************************************************************
 * @fn                - v_oledDrawBar
 * @brief             - Draw a horizontal progress bar with an outline.
 * @param[in]         - u1_x       : left position
 * @param[in]         - u1_y       : top position
 * @param[in]         - u1_width   : total width in pixels
 * @param[in]         - u1_height  : total height in pixels
 * @param[in]         - u1_percent : filled part, 0 to 100
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledDrawBar(uint8_t u1_x, uint8_t u1_y, uint8_t u1_width,
                   uint8_t u1_height, uint8_t u1_percent)
{
    uint8_t u1t_column = 0u;
    uint8_t u1t_row = 0u;
    uint8_t u1t_fill = 0u;
    uint8_t u1t_value = u1_percent;

    if (u1t_value > (uint8_t)OLED_PERCENT_FULL)
    {
        u1t_value = (uint8_t)OLED_PERCENT_FULL;
    }

    u1t_fill = (uint8_t)(((uint16_t)u1_width * (uint16_t)u1t_value)
             / (uint16_t)OLED_PERCENT_FULL);

    for (u1t_column = 0u; u1t_column < u1_width; u1t_column++)
    {
        v_oledSetPixel((uint8_t)(u1_x + u1t_column), u1_y);
        v_oledSetPixel((uint8_t)(u1_x + u1t_column),
                       (uint8_t)(u1_y + u1_height - 1u));
    }

    for (u1t_row = 0u; u1t_row < u1_height; u1t_row++)
    {
        v_oledSetPixel(u1_x, (uint8_t)(u1_y + u1t_row));
        v_oledSetPixel((uint8_t)(u1_x + u1_width - 1u), (uint8_t)(u1_y + u1t_row));
    }

    for (u1t_column = 0u; u1t_column < u1t_fill; u1t_column++)
    {
        for (u1t_row = 2u; u1t_row < (uint8_t)(u1_height - 2u); u1t_row++)
        {
            v_oledSetPixel((uint8_t)(u1_x + u1t_column), (uint8_t)(u1_y + u1t_row));
        }
    }
}

/*********************************************************************
 * @fn                - v_oledFlush
 * @brief             - Push the frame buffer to the screen one page at a
 *                      time. Each page carries its own address command, so
 *                      the eight transfers stay independent of each other.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_oledFlush(void)
{
    uint8_t u1t_command[OLED_CMD_FRAME_LENGTH];
    uint8_t u1t_page = 0u;

    for (u1t_page = 0u; u1t_page < (uint8_t)OLED_PAGE_COUNT; u1t_page++)
    {
        u1t_command[0] = (uint8_t)((uint8_t)OLED_CMD_PAGE_BASE + u1t_page);
        u1t_command[1] = (uint8_t)OLED_CMD_COLUMN_LOW;
        u1t_command[2] = (uint8_t)OLED_CMD_COLUMN_HIGH;

        v_oledSendCommandList(u1t_command, (uint16_t)OLED_CMD_FRAME_LENGTH);

        (void)u1_i2cWriteBlock((uint8_t)OLED_ADDRESS, (uint8_t)OLED_CONTROL_DATA,
                               &u1g_oledBuffer[(uint16_t)u1t_page * OLED_PAGE_SIZE],
                               (uint16_t)OLED_PAGE_SIZE);
    }
}
