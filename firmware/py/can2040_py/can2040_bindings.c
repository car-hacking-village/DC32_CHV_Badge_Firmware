#include <string.h>

// Include MicroPython API.
#include <py/runtime.h>
#include <py/obj.h>
#include <py/gc.h>
#include <py/mphal.h>


// rp2040 specifics
#include <RP2040.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include <hardware/claim.h>

// non-micropython stuff
#include <can2040.h>

#ifndef STATIC
#define STATIC static
#endif

STATIC const mp_obj_type_t mp_type_caninterface;

// We have this wrapper object for the internals for the same reason micropython does
// We need to lie about it's type while smuggling additional variables in
typedef struct canbus_internal {
    struct can2040 internal;
    mp_obj_t * recv_queue;
    mp_fun_1_t pop;
    mp_fun_2_t push;
} canbus_internal_t;

typedef struct {
    mp_obj_base_t base;
    canbus_internal_t bus;
    uint32_t sys_clock;
    uint32_t bitrate;
    uint32_t gpio_rx;
    uint32_t gpio_tx;
    uint32_t pio_num;
    // uint32_t max_retries;
    bool started;
} mp_obj_can_interface_t;

// PRIVATE
STATIC mp_obj_can_interface_t *mp_can_obj_0;
STATIC mp_obj_can_interface_t *mp_can_obj_1;

static void
can2040_cb(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg)
{
    canbus_internal_t* bus = (canbus_internal_t*)cd;
    if (notify == CAN2040_NOTIFY_RX)
    {
        // We shouldn't need to worry about packing because it is all multiples of 64.
        // So no matter the architecture it will be on cache lines
        // Also we probably don't need to care about an exception being thrown since 
        // we don't set the flags which would cause the exception

        if (msg == NULL) {
            mp_printf(MICROPY_ERROR_PRINTER, "msg is null????\n");
            return;
        }

        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) { // This setjumps
            mp_obj_t msg_bytes = mp_obj_new_bytes((uint8_t*)msg, sizeof(struct can2040_msg));
            mp_call_function_2(
                bus->push,
                bus->recv_queue,
                msg_bytes
            );
            nlr_pop(); // this will longjump if exception
            return;
        } else { // enter here on exception
            mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
            mp_printf(MICROPY_ERROR_PRINTER, "CAN RX msg queue full\n");
        }
    }
    if (notify & CAN2040_NOTIFY_ERROR)
    {
        mp_printf(MICROPY_ERROR_PRINTER, "Error...\n");
    }

}

static void can2040_internal_pio0_irq_handler(void)
{
    // lock the python internals while we run our irq to make sure we aren't interupted!
    mp_sched_lock();
    // gc_lock(); // Can't lock the garbage collector because we are allocating memory :(

    // handle the irq
    can2040_pio_irq_handler(&(mp_can_obj_0->bus.internal));

    // unlock in reverse order!
    // gc_unlock();
    mp_sched_unlock();
}

static void can2040_internal_pio1_irq_handler(void)
{
    // lock the python internals while we run our irq to make sure we aren't interupted!
    mp_sched_lock();
    // gc_lock(); // Can't lock the garbage collector because we are allocating memory :(

    // handle the irq
    can2040_pio_irq_handler(&(mp_can_obj_1->bus.internal));

    // unlock in reverse order!
    // gc_unlock();
    mp_sched_unlock();
}

STATIC mp_obj_t can_init_helper(mp_obj_can_interface_t *self ) {

    pio_hw_t * pio = (self->pio_num == 0) ? pio0_hw : pio1_hw;
    uint pio_irq = (self->pio_num == 0) ? PIO0_IRQ_0 : PIO1_IRQ_0;

    // Setup canbus internal structure
    can2040_setup(&self->bus.internal, self->pio_num);
    can2040_callback_config(&self->bus.internal, can2040_cb);

    // disable the irq while configuring it
    irq_set_enabled(pio_irq, false);

    // claim all the pio sm irqs
    uint8_t is_claimed = 0;
    for (uint8_t i=0; i < 4 && !is_claimed; i ++) {
        if (pio_sm_is_claimed(pio, i)) {
            is_claimed = i;
            break;
        }
        pio_sm_claim(pio, i);
    }

    // if we can't own the whole PIO, then we don't want any of it.
    if (0 != is_claimed) {
        for (uint8_t i = 0; i < is_claimed; i ++) {
            pio_sm_unclaim(pio, i);
        }

        irq_set_enabled(pio_irq, true); // reenable since we don't own it
        mp_raise_ValueError("StateMachine claimed by external resource!");
    }


    irq_handler_t handler = irq_get_exclusive_handler(pio_irq);
    if (handler != NULL) {
        // this is the default MicroPython irq handler... hopefully
        irq_remove_handler(pio_irq, handler);
    }

    // configure with our IRQ handler
    
    irq_set_exclusive_handler(pio_irq, 
        (self->pio_num == 0) ? can2040_internal_pio0_irq_handler : can2040_internal_pio1_irq_handler);
    irq_set_priority(pio_irq, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(pio_irq, true);

    // Start canbus
    can2040_start(&self->bus.internal, self->sys_clock, self->bitrate, self->gpio_rx, self->gpio_tx);
    self->started = true;


    return MP_OBJ_FROM_PTR(self);
}


STATIC mp_obj_t mp_can_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    enum { ARG_bitrate, ARG_sysclock, ARG_gpiorx, ARG_gpiotx, ARG_pionum, ARG_max_retries};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_bitrate,  MP_ARG_INT,   {.u_int = 500000} },
        { MP_QSTR_sysclock, MP_ARG_INT,   {.u_int = 125000000} },
        { MP_QSTR_gpiorx,   MP_ARG_INT,   {.u_int = 17} },
        { MP_QSTR_gpiotx,   MP_ARG_INT,   {.u_int = 18} },
        { MP_QSTR_pionum,   MP_ARG_INT,   {.u_int = 1}  },
        // { MP_QSTR_max_retries, MP_ARG_INT,{.u_int = 10}  },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t pio_num = args[ARG_pionum].u_int;
    if ((pio_num != 0) && (pio_num != 1)) {
        mp_raise_ValueError("Invalid PIO number. Must be 0 or 1");
    }

    // setup the object
    mp_obj_can_interface_t *self;
    if ((pio_num == 0) && (mp_can_obj_0 != NULL)) {
        self = mp_can_obj_0;
    } else if ((pio_num == 1) && (mp_can_obj_1 != NULL)) {
        self = mp_can_obj_1;
    } else {
        // create a new obj and save it for later
        self = m_new_obj(mp_obj_can_interface_t);
        self->base.type = &mp_type_caninterface;
        if (pio_num == 0) {
            mp_can_obj_0 = self;
        } else {
            mp_can_obj_1 = self;
        }

        // create a new deque and call it's make_new (__init__)
        mp_obj_t deque_args[2] = {mp_const_empty_tuple, mp_obj_new_int(10)};
        self->bus.recv_queue = MP_OBJ_TYPE_GET_SLOT(&mp_type_deque, make_new)(&mp_type_deque, 2, 0, deque_args);
        if (self->bus.recv_queue == NULL) {
            mp_raise_ValueError("Unable to allocate queue");
        }


        // add direct function calls so that we don't need to do lookup every time we use them
        mp_map_t *locals_map = &MP_OBJ_TYPE_GET_SLOT(&mp_type_deque, locals_dict)->map;
        mp_map_elem_t * pop = mp_map_lookup(locals_map, MP_ROM_QSTR(MP_QSTR_popleft), MP_MAP_LOOKUP);
        if ((pop == NULL) || (pop->value == MP_OBJ_NULL)) {
            mp_raise_ValueError("Unable to locate deque functions");
        } else {
            self->bus.pop = (mp_fun_1_t)pop->value;
        }
        mp_map_elem_t * push = mp_map_lookup(locals_map, MP_ROM_QSTR(MP_QSTR_append), MP_MAP_LOOKUP);
        if ((push == NULL) || (push->value == MP_OBJ_NULL)) {
            mp_raise_ValueError("Unable to locate deque functions");
        }  else {
            self->bus.push = (mp_fun_2_t)push->value;
        }

        self->gpio_rx = args[ARG_gpiorx].u_int;
        self->gpio_tx = args[ARG_gpiotx].u_int;
        self->sys_clock = args[ARG_sysclock].u_int;
        self->bitrate = args[ARG_bitrate].u_int;
        self->pio_num = pio_num;
        // self->max_retries = args[ARG_max_retries].u_int;

        if (pio_num == 0) {
            mp_can_obj_0 = self;
        } else {
            mp_can_obj_1 = self;
        }
        
        // Need to setup here and set the PIO interface
        can_init_helper(self);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t mp_can_send_helper(mp_obj_can_interface_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_id, ARG_dlc, ARG_data, ARG_extframe, ARG_remote};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,       MP_ARG_REQUIRED | MP_ARG_INT,   {.u_int = 0} },
        { MP_QSTR_dlc,      MP_ARG_REQUIRED | MP_ARG_INT,   {.u_int = 0} },
        { MP_QSTR_data,     MP_ARG_REQUIRED | MP_ARG_OBJ,   {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_extended, MP_ARG_BOOL,                    {.u_bool = false} },
        { MP_QSTR_remote,   MP_ARG_BOOL,                    {.u_bool = false} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (!self->started) {
        mp_raise_ValueError("Canbus is stopped");
    }

    struct can2040_msg response;

    // if an extended frame
    if (args[ARG_extframe].u_bool == true || args[ARG_id].u_int > 0x7ff) {
        response.id = (args[ARG_id].u_int & 0x1FFFFFFF) | CAN2040_ID_EFF;
    } else {
        response.id = args[ARG_id].u_int & 0x7FF;
    }

    if (args[ARG_remote].u_bool == true) {
        response.id |= CAN2040_ID_RTR;
    }

    response.dlc = args[ARG_dlc].u_int & 0xFF;

    mp_buffer_info_t data;
    mp_get_buffer_raise(args[ARG_data].u_obj, &data, MP_BUFFER_READ);
    for(int i = 0; i < data.len || i < 8; i++) {
        response.data[i] = ((uint8_t*)data.buf)[i];
    }

    // Debugging
    // mp_printf(MICROPY_ERROR_PRINTER, "%d - %d\n", response.id, response.dlc);
    // for(int i = 0; i < 8; i++) {
    //     mp_printf(MICROPY_ERROR_PRINTER, "%d\n", ((uint8_t*)data.buf)[i]);
    // }

    can2040_transmit(&self->bus.internal, &response);

    return mp_const_none;
}
STATIC mp_obj_t mp_can_send(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    return mp_can_send_helper(MP_OBJ_TO_PTR(pos_args[0]), n_args - 1, pos_args + 1, kw_args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(canbus_send_obj, 1 , mp_can_send);

STATIC mp_obj_t mp_can_recv_helper(mp_obj_can_interface_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args, bool block) {

    struct can2040_msg msg;

    if ((!self->started) || (self->bus.pop == NULL)) {
        mp_raise_ValueError("Canbus is stopped");
    }

    mp_obj_t res;
    bool contin = true;
    do {
        // This is disgusting. Holy cow.
        // Never do this in real life.
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) { // This setjumps
            res = mp_call_function_1(self->bus.pop,self->bus.recv_queue);
            nlr_pop(); // this will longjump if exception
            contin = false;
        } else { // enter here on exception
            if (!block) {
                // mp_raise_ValueError("Nothing to recv");
                return MP_OBJ_NULL;
            } else {
                mp_hal_delay_ms(1);
            }
        }
    } while(contin);
    
    // parse the message here
    mp_buffer_info_t data;
    mp_get_buffer_raise(res, &data, MP_BUFFER_READ);
    memcpy(&msg, data.buf, data.len);


    mp_obj_t ret = mp_obj_new_tuple(5, NULL);

    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
    tuple->items[0] = MP_OBJ_NEW_SMALL_INT(msg.id & ~(CAN2040_ID_EFF | CAN2040_ID_RTR));
    tuple->items[1] = MP_OBJ_NEW_SMALL_INT(msg.dlc);
    tuple->items[2] = mp_obj_new_bytes(msg.data, sizeof(msg.data));
    tuple->items[3] = mp_obj_new_bool(msg.id & CAN2040_ID_EFF);
    tuple->items[4] = mp_obj_new_bool(msg.id & CAN2040_ID_RTR);

    // Return the result
    return ret;
}

STATIC mp_obj_t mp_can_recv(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    return mp_can_recv_helper(MP_OBJ_TO_PTR(pos_args[0]), n_args - 1, pos_args + 1, kw_args, true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(canbus_recv_obj, 1 , mp_can_recv);

STATIC mp_obj_t mp_can_try_recv(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    return mp_can_recv_helper(MP_OBJ_TO_PTR(pos_args[0]), n_args - 1, pos_args + 1, kw_args, false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(canbus_try_recv_obj, 1 , mp_can_try_recv);


STATIC mp_obj_t mp_can_start(mp_obj_t self_in) {
    mp_obj_can_interface_t*self = MP_OBJ_TO_PTR(self_in);

    if (!self->started) {
        memset(&(self->bus.internal.stats), 0, sizeof(struct can2040_stats));

        can2040_start(&(self->bus.internal), self->sys_clock, self->bitrate, self->gpio_rx, self->gpio_tx);
        self->started = true;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(canbus_start_obj, mp_can_start);

STATIC mp_obj_t mp_can_stop(mp_obj_t self_in) {
    mp_obj_can_interface_t*self = MP_OBJ_TO_PTR(self_in);

    if (self->started) {
        can2040_stop(&(self->bus.internal));
        self->started = false;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(canbus_stop_obj, mp_can_stop);

STATIC mp_obj_t canbus_bitrate(mp_obj_t self_in, mp_obj_t new_bitrate_obj) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    if (new_bitrate_obj != mp_const_none) {
        mp_int_t new_bitrate;
        if(mp_obj_get_int_maybe(new_bitrate_obj, &new_bitrate)) {
            self->bitrate = new_bitrate;
        } else {
            // TODO: should we do something here?
        }
    }
    return mp_obj_new_int(self->bitrate);
}

MP_DEFINE_CONST_FUN_OBJ_2(canbus_bitrate_obj, canbus_bitrate);

STATIC mp_obj_t canbus_gpio_rx(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->gpio_rx);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_gpio_rx_obj, canbus_gpio_rx);

STATIC mp_obj_t canbus_gpio_tx(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->gpio_tx);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_gpio_tx_obj, canbus_gpio_tx);

STATIC mp_obj_t canbus_pio_num(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->pio_num);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_pio_num_obj, canbus_pio_num);

STATIC mp_obj_t canbus_started(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(self->started);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_started_obj, canbus_started);

STATIC mp_obj_t canbus_state(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->bus.internal.report_state);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_state_obj, canbus_state);

STATIC mp_obj_t canbus_retransmissions(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);

    struct can2040_stats stats = {0};

    can2040_get_statistics(&self->bus.internal, &stats);

    return mp_obj_new_int(stats.tx_attempt - stats.tx_total);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_retransmissions_obj, canbus_retransmissions);

STATIC mp_obj_t canbus_errors(mp_obj_t self_in) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);

    struct can2040_stats stats = {0};

    can2040_get_statistics(&self->bus.internal, &stats);

    return mp_obj_new_int(stats.parse_error);
}

MP_DEFINE_CONST_FUN_OBJ_1(canbus_errors_obj, canbus_errors);

STATIC mp_obj_t canbus_get_msg(mp_obj_t self_in, mp_obj_t counter_obj) {
    mp_obj_can_interface_t *self = MP_OBJ_TO_PTR(self_in);
    if (counter_obj != mp_const_none) {
        mp_int_t counter;
        mp_obj_t list = mp_obj_new_list(0, NULL);
        if(mp_obj_get_int_maybe(counter_obj, &counter)) {
            
            // if (counter % 3 == 0) {
            //     mp_obj_t ret = mp_obj_new_tuple(3, NULL);
            //     mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
            //     tuple->items[0] = MP_OBJ_NEW_SMALL_INT(0xc32);
            //     tuple->items[1] = MP_OBJ_NEW_SMALL_INT(8);
            //     tuple->items[2] = mp_obj_new_bytes((byte*)"flag{I'm not sure what to do", 8);
            //     mp_obj_list_append(list, ret);
            // }
            // if (counter % 5 == 0) {
            //     mp_obj_t ret = mp_obj_new_tuple(3, NULL);
            //     mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
            //     tuple->items[0] = MP_OBJ_NEW_SMALL_INT(0xb77);
            //     tuple->items[1] = MP_OBJ_NEW_SMALL_INT(6);
            //     tuple->items[2] = mp_obj_new_bytes((byte*)"never have I seen such a weird", 6);
            //     mp_obj_list_append(list, ret);
            // }
            // if (counter % 7 == 0) {
            //     mp_obj_t ret = mp_obj_new_tuple(3, NULL);
            //     mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
            //     tuple->items[0] = MP_OBJ_NEW_SMALL_INT(0x7c3);
            //     tuple->items[1] = MP_OBJ_NEW_SMALL_INT(6);
            //     tuple->items[2] = mp_obj_new_bytes((byte*)"gunna be the worst way to die", 6);
            //     mp_obj_list_append(list, ret);
            // }
            // if (counter % 11 == 0) {
            //     mp_obj_t ret = mp_obj_new_tuple(3, NULL);
            //     mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
            //     tuple->items[0] = MP_OBJ_NEW_SMALL_INT(0xc2c);
            //     tuple->items[1] = MP_OBJ_NEW_SMALL_INT(5);
            //     tuple->items[2] = mp_obj_new_bytes((byte*)"give me a break, this is ridiculous", 5);
            //     mp_obj_list_append(list, ret);
            // }
            // if (counter % 13 == 0) {
            //     mp_obj_t ret = mp_obj_new_tuple(3, NULL);
            //     mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
            //     tuple->items[0] = MP_OBJ_NEW_SMALL_INT(0x20d);
            //     tuple->items[1] = MP_OBJ_NEW_SMALL_INT(7);
            //     tuple->items[2] = mp_obj_new_bytes((byte*)"you up}to trying this again?", 7);
            //     mp_obj_list_append(list, ret);
            // }

            return list;
        } else {
            return mp_const_none;
        }
    }
    return mp_obj_new_int(self->bitrate);
}

MP_DEFINE_CONST_FUN_OBJ_2(canbus_get_msg_obj, canbus_get_msg);


STATIC const mp_rom_map_elem_t canhack_caninterface_locals_dict_table[] = {
    // { MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&mp_can_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&canbus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&canbus_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&canbus_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&canbus_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_bitrate), MP_ROM_PTR(&canbus_bitrate_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpiorx), MP_ROM_PTR(&canbus_gpio_rx_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpiotx), MP_ROM_PTR(&canbus_gpio_tx_obj) },
    { MP_ROM_QSTR(MP_QSTR_pionum), MP_ROM_PTR(&canbus_pio_num_obj) },
    { MP_ROM_QSTR(MP_QSTR_started), MP_ROM_PTR(&canbus_started_obj) },
    { MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&canbus_state_obj) },
    { MP_ROM_QSTR(MP_QSTR_retransmissions), MP_ROM_PTR(&canbus_retransmissions_obj)},
    { MP_ROM_QSTR(MP_QSTR_errors), MP_ROM_PTR(&canbus_errors_obj)},
    { MP_ROM_QSTR(MP_QSTR_try_recv), MP_ROM_PTR(&canbus_try_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_msg), MP_ROM_PTR(&canbus_get_msg_obj) },
};
STATIC MP_DEFINE_CONST_DICT(canhack_caninterface_locals_dict, canhack_caninterface_locals_dict_table);

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_caninterface,
    MP_QSTR_canbus,
    MP_TYPE_FLAG_NONE,
    make_new, mp_can_make_new,
    locals_dict, &canhack_caninterface_locals_dict
    );

STATIC const mp_rom_map_elem_t can_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__canbus) },
    { MP_ROM_QSTR(MP_QSTR_bus), MP_ROM_PTR(&mp_type_caninterface) },
};
STATIC MP_DEFINE_CONST_DICT(can_module_globals, can_module_globals_table);

const mp_obj_module_t mp_module_canhack = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&can_module_globals,
};

// Register the module 'can' and make it available in Python
MP_REGISTER_MODULE(MP_QSTR__canbus, mp_module_canhack);
