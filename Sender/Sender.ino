#define _F1_PIN 6
#define _F2_PIN 8
#define _F3_PIN 10
#define _F4_PIN 12
#define _F5_PIN 5
#define _F6_PIN 7
#define _F7_PIN 9
#define _F8_PIN 11
#define _BKL_PIN 2
#define _LFI_PORTB 5

void matrix_scan();
void key_scan();

bool matrix_status[8] = {0, };
bool key_pressed[8] = {0, };

const uint8_t keyp_list[8] = {
    _F1_PIN, _F2_PIN, _F3_PIN, _F4_PIN,
    _F5_PIN, _F6_PIN, _F7_PIN, _F8_PIN,
};

const bool bit[8][8] = {
    {0, 0, 1, 1, 1, 1, 0, 0}, // F5
    {0, 1, 0, 1, 1, 0, 1, 0}, // F1
    {0, 1, 1, 0, 0, 1, 1, 0}, // F6
    {0, 1, 1, 1, 1, 1, 1, 0}, // F2
    {1, 0, 0, 1, 1, 0, 0, 1}, // F7
    {1, 0, 1, 0, 0, 1, 0, 1}, // F3
    {1, 0, 1, 1, 1, 1, 0, 1}, // F8
    {1, 1, 0, 0, 0, 0, 1, 1}, // F4
};

#define _BIT_STREAM_SIZE 9
struct _bit_stream
{
    int8_t queue[_BIT_STREAM_SIZE][2];
    int8_t queue_front;
    int8_t queue_rear;
    int8_t queue_size;

    _bit_stream()
    {
        queue[_BIT_STREAM_SIZE][2] = {{0, }, };
        queue_front = 0;
        queue_rear = 0;
        queue_size = 0;
    }

    void queue_push(uint8_t pin, uint8_t status)
    {
        if ((queue_rear + 1) % _BIT_STREAM_SIZE == queue_front)
        {
            return;
        }
        queue_size++;
        queue_rear = (queue_rear + 1) % _BIT_STREAM_SIZE;
        queue[queue_rear][0] = pin;
        queue[queue_rear][1] = status;
    }

    int8_t queue_pop()
    {
        if (queue_rear == queue_front)
        {
            return 0;
        }
        queue_size--;
        queue_front = (queue_front + 1) % _BIT_STREAM_SIZE;
        int8_t target_status = queue[queue_front][0];
        if (!queue[queue_front][1])
        {
            target_status *= -1;
        }
        queue[queue_front][0] = 0;
        queue[queue_front][1] = 0;

        return target_status;
    }
} bit_stream;

#define _BACKLIGHT_PWM 175
void setup()
{
    pinMode(_BKL_PIN, OUTPUT);
    DDRB |= (1 << _LFI_PORTB);

    for (int8_t idx = 0; idx < 8; idx++)
    {
        pinMode(keyp_list[idx], INPUT_PULLUP);
    }

    analogWrite(_BKL_PIN, _BACKLIGHT_PWM);
}

void loop()
{
    matrix_scan();
    lfi_bit_send();
}

void matrix_scan()
{
    for (int8_t idx = 0; idx < 8; idx++)
    {
        matrix_status[idx] = !digitalRead(keyp_list[idx]);

        if (key_pressed[idx] != matrix_status[idx])
        {
            bit_stream.queue_push(keyp_list[idx], matrix_status[idx]);
            key_pressed[idx] = matrix_status[idx];
        }
    }
}

#define _BIT_WIDTH 30000
void lfi_bit_send()
{
    if (bit_stream.queue_size == 0)
    {
        return;
    }

    bool bit_buffer[10] = {0, };
    bool pressed = 0;
    int8_t key = bit_stream.queue_pop();

    if (key < 0)
    {
        return;
    }

    bit_buffer[0] = pressed;
    bit_buffer[9] = pressed;

    for (int8_t idx = 1; idx <= 8; idx++)
    {
        bit_buffer[idx] = bit[key - 5][idx - 1];
    }

    unsigned long pre_time = 0;
    for (int8_t idx = 0; idx < 10; idx++)
    {
        while (1)
        {
            unsigned long cur_time = micros();
            if (micros() - pre_time >= _BIT_WIDTH)
            {
                if (bit_buffer[idx])
                {
                    PORTB |= (1 << _LFI_PORTB);
                } else {
                    PORTB &= ~(1 << _LFI_PORTB);
                }
                pre_time = micros();

                break;
            }
        }
    }
    PORTB &= ~(1 << _LFI_PORTB);
}