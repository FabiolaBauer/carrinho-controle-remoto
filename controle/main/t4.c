/**
 * @file t4.c
 * @brief Teste T4 (RF06): sequência de pacotes problemáticos (Seção 7, item 8)
 *
 * Fora do build de teste (CONFIG_FW_TEST_T4) este arquivo fica vazio.
 *
 * Esperado no carrinho:
 *  - OLD_SEQ, BAD_RANGE, BAD_CRC: rejeitados e contados em rejected_cnt (+3).
 *    São 3 ciclos de 50 ms sem pacote válido (150 ms < LINK_TIMEOUT_MS), então
 *    o enlace não cai.
 *  - FWD_MAX e REVERSE: pacotes VÁLIDOS. O carrinho anda (rodas fora do chão!):
 *    a subida segue a rampa (0 -> 100% em 0,5 s) e a inversão direta passa por
 *    zero e espera REVERSE_DWELL_MS antes da ré. Não contam em rejected_cnt.
 */

#include "t4.h"

#if CONFIG_FW_TEST_T4

#include "config.h"
#include "crc8.h"

/* Escritos só pelo timer de envio; lidos pela UI */
static t4_step_t s_step = T4_WAIT;                 /* passo em execução */
static volatile t4_step_t s_sent_step = T4_WAIT;   /* passo do último pacote enviado */
static uint32_t s_step_start_ms = 0;

static void enter(t4_step_t step, uint32_t now_ms)
{
    s_step = step;
    s_step_start_ms = now_ms;
}

static void set_crc(cmd_packet_t *pkt)
{
    pkt->crc = crc8_calc((const uint8_t *)pkt, sizeof(cmd_packet_t) - 1);
}

void t4_apply(uint32_t now_ms, cmd_packet_t *pkt)
{
    /* Registra qual passo gerou este pacote (lido pela UI para o log) */
    s_sent_step = s_step;

    switch (s_step) {
        case T4_WAIT:
            if (now_ms < T4_START_DELAY_MS) {
                return;
            }
            enter(T4_OLD_SEQ, now_ms);
            s_sent_step = T4_OLD_SEQ;
            /* fallthrough: o primeiro pacote da sequência sai neste ciclo */
            __attribute__((fallthrough));

        case T4_OLD_SEQ:
            /* seq anterior ao último aceito pelo carrinho */
            pkt->seq = (uint16_t)(pkt->seq - T4_OLD_SEQ_BACK);
            pkt->x = 0;
            pkt->y = 0;
            set_crc(pkt);
            enter(T4_BAD_RANGE, now_ms);
            return;

        case T4_BAD_RANGE:
            pkt->x = T4_BAD_X;    /* fora de PROTO_AXIS_MIN..PROTO_AXIS_MAX */
            pkt->y = 0;
            set_crc(pkt);
            enter(T4_BAD_CRC, now_ms);
            return;

        case T4_BAD_CRC:
            pkt->x = 0;
            pkt->y = 0;
            set_crc(pkt);
            pkt->crc ^= 0xFF;     /* CRC errado */
            enter(T4_FWD_MAX, now_ms);
            return;

        case T4_FWD_MAX:
            if (now_ms - s_step_start_ms >= T4_FWD_MS) {
                enter(T4_REVERSE, now_ms);
                s_sent_step = T4_REVERSE;
                /* inversão direta: este mesmo ciclo já manda y = -100 */
                pkt->x = 0;
                pkt->y = PROTO_AXIS_MIN;
                set_crc(pkt);
                return;
            }
            pkt->x = 0;
            pkt->y = PROTO_AXIS_MAX;
            set_crc(pkt);
            return;

        case T4_REVERSE:
            if (now_ms - s_step_start_ms >= T4_REV_MS) {
                enter(T4_DONE, now_ms);
                s_sent_step = T4_DONE;
                return;           /* pacote do joystick, sem alteração */
            }
            pkt->x = 0;
            pkt->y = PROTO_AXIS_MIN;
            set_crc(pkt);
            return;

        case T4_DONE:
        default:
            return;
    }
}

t4_step_t t4_current_step(void)
{
    return s_sent_step;
}

const char *t4_step_name(t4_step_t step)
{
    switch (step) {
        case T4_WAIT:      return "aguardando";
        case T4_OLD_SEQ:   return "1/5 sequencia antiga";
        case T4_BAD_RANGE: return "2/5 x=120 (fora de faixa)";
        case T4_BAD_CRC:   return "3/5 CRC errado";
        case T4_FWD_MAX:   return "4/5 frente maxima (valido, rampa)";
        case T4_REVERSE:   return "5/5 inversao direta +100 -> -100 (valido, passa por zero)";
        case T4_DONE:      return "concluido, voltando ao joystick";
        default:           return "?";
    }
}

#endif // CONFIG_FW_TEST_T4
