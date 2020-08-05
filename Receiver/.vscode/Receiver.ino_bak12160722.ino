#define CDS_PIN A0

void keyboard_scan(uint8_t key);
void cds_scan();
int8_t decode_bits();

uint16_t static_avg = 0;
uint16_t static_avg_cnt = 0;

const bool bit[8][8] = {
    {0, 1, 0, 1, 1, 0, 1, 0}, // F1
    {0, 1, 1, 1, 1, 1, 1, 0}, // F2
    {1, 0, 1, 0, 0, 1, 0, 1}, // F3
    {1, 1, 0, 0, 0, 0, 1, 1}, // F4
    {0, 0, 1, 1, 1, 1, 0, 0}, // F5
    {0, 1, 1, 0, 0, 1, 1, 0}, // F6
    {1, 0, 0, 1, 1, 0, 0, 1}, // F7
    {1, 0, 1, 1, 1, 1, 0, 1}, // F8
};

#define _CIRCULAR_QUEUE_SIZE 11
struct _circular_queue {
    int16_t queue[_CIRCULAR_QUEUE_SIZE];
    int8_t queue_front;
    int8_t queue_rear;
    int8_t queue_size;

    _circular_queue() {
        queue[_CIRCULAR_QUEUE_SIZE] = {{0, }, };
        queue_front = 0;
        queue_rear = 0;
        queue_size = 0;
    }
  
    void queue_push(bool status, uint16_t value) {
        if((queue_rear + 1) % _CIRCULAR_QUEUE_SIZE == queue_front) {
            return ;
        }
        queue_size++;
        queue_rear = (queue_rear + 1) % _CIRCULAR_QUEUE_SIZE;
        queue[queue_rear][0] = status;
    }

    bool queue_pop() {
        if(queue_rear == queue_front) {
            return 0;
        }
        queue_size--;
        queue_front = (queue_front + 1) % _CIRCULAR_QUEUE_SIZE;
        bool target_status = queue[queue_front];
        queue[queue_front] = 0;

        return target_status;
    }
} bit_stream;

#define _BIT_QUEUE_SIZE 
struct _bit_queue {
    int16_t queue[_BIT_QUEUE_SIZE][2];
    int8_t queue_front;
    int8_t queue_rear;
    int8_t queue_size;

    _bit_queue() {
        queue[_BIT_QUEUE_SIZE][2] = {{0, }, };
        queue_front = 0;
        queue_rear = 0;
        queue_size = 0;
    }
  
    void queue_push(bool status, uint16_t value) {
        if((queue_rear + 1) % _BIT_QUEUE_SIZE == queue_front) {
            return ;
        }
        queue_size++;
        queue_rear = (queue_rear + 1) % _BIT_QUEUE_SIZE;
        queue[queue_rear][0] = status;
        queue[queue_rear][1] = value;
    }

    bool queue_pop() {
        if(queue_rear == queue_front) {
            return 0;
        }
        queue_size--;
        queue_front = (queue_front + 1) % _BIT_QUEUE_SIZE;
        bool target_status = queue[queue_front][0];
        queue[queue_front][0] = 0;
        queue[queue_front][1] = 0;

        return target_status;
    }
} bit_tphase;

void setup() {
    Serial.begin(115200);
    pinMode(CDS_PIN, INPUT);
    while(millis() <= 3000) {
        uint16_t value = analogRead(CDS_PIN);
        static_avg += (value / 10);
        static_avg_cnt++;
    }
}

void loop() {
    cds_scan();
    decode_bits();
}

unsigned long pre_time = 0;
void cds_scan() {
    while(1) {
        if(micros() - pre_time >= 10000) {
            uint16_t value = analogRead(CDS_PIN);
            if(value > ((static_avg * 10) / static_avg_cnt + 10)) {
                bit_tphase.queue_push(1, value);
            } else {
                bit_tphase.queue_push(0, value);
            }
            if(bit_tphase.queue_size == _BIT_QUEUE_SIZE - 1) {
                uint16_t bit_status = 0;
                for(int8_t idx = 1; idx < _BIT_QUEUE_SIZE; idx++) {
                    bit_status += bit_tphase.queue[(bit_tphase.queue_front + idx) % _BIT_QUEUE_SIZE][1];
                    Serial.print(bit_status);
                    Serial.print(" ");
                }
                bit_status /= 4;
                bit_status = (bit_status >= ((static_avg * 10) / static_avg_cnt + 10))

                if(bit_status == 1) {
                    bit_stream.queue_push(1, value);
                } else if (bit_status == 0) {
                    bit_stream.queue_push(0, value);
                }

                if(bit_status == 1 || bit_status == 0) {
                    for(int8_t idx = 0; idx < _BIT_QUEUE_SIZE -1 ; idx++) {
                        bit_tphase.queue_pop();
                    }
                } else {
                    bit_tphase.queue_pop();
                }
            }
            
            pre_time = micros();

            break;
        }
    }
}

int8_t decode_bits() {
    if(bit_stream.queue_size != _CIRCULAR_QUEUE_SIZE - 1) {
        return -1;
    }

    int16_t bit_buffer[11][2] = {{0, }, };
    int8_t cnt = 10;
    int8_t queue_idx = bit_stream.queue_front;
    while(cnt >= 0) {
        queue_idx = (queue_idx + 1) % _CIRCULAR_QUEUE_SIZE;
        bit_buffer[cnt][0] = bit_stream.queue[queue_idx][0];
        bit_buffer[cnt][1] = bit_stream.queue[queue_idx][1];
        cnt--;
    }

    uint8_t stream_matched = 0;
    uint8_t bit_or_ed[5] = {0, };
    int8_t key = -1;
    for(int8_t idx = 0; idx < 5; idx++) {
        stream_matched += (bit_buffer[idx][0] == bit_buffer[9 - idx][0]);
        bit_or_ed[idx] = (bit_buffer[idx][0] || bit_buffer[9 - idx][0]);
    }
    if(stream_matched >= 2 && (bit_or_ed[0] != 1)) {
        for(int8_t idx = 0; idx < 5; idx++) {
            Serial.print(bit_or_ed[idx]);
            Serial.print(" ");
        }
        Serial.println("");
        for(int8_t idx = 0; idx < 8; idx++) {
            uint8_t matched = 0;
            for(int8_t jdx = 0; jdx < 4; jdx++) {
                if(bit_or_ed[jdx + 1] == bit[idx][jdx]) {
                    matched++;
                }
            }
            if(matched == 3) {
                key = idx;
            }
        }
        for(int8_t idx = 0; idx < _CIRCULAR_QUEUE_SIZE; idx++) {
            bit_stream.queue_pop();
        }
    }

    if(bit_stream.queue_size == _CIRCULAR_QUEUE_SIZE - 1) {
        bit_stream.queue_pop();
    }
    
    return key;
}
