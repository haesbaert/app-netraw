/*
 * Copyright (c) 2023 Christiano Haesbaert <haesbaert@haesbaert.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <uk/essentials.h>
#include <uk/netdev.h>

#define PKT_BUFLEN 2048

static void
errx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

	exit(1);
}

static void
millisleep(unsigned int millisec)
{
	struct timespec ts;
	int ret;

	ts.tv_sec = millisec / 1000;
	ts.tv_nsec = (millisec % 1000) * 1000000;
	do
		ret = nanosleep(&ts, &ts);
	while (ret && errno == EINTR);
}

static void
dump_data(const void *s, size_t len)
{
	size_t i, j;
	const uint8_t *p = s;

	for (i = 0; i < len; i += 16) {
		printf("%.4zu: ", i);
		for (j = i; j < i + 16; j++) {
			if (j < len)
				printf("%02x ", p[j]);
			else
				printf("   ");
		}
		printf(" ");
		for (j = i; j < i + 16; j++) {
			if (j < len) {
				if  (isascii(p[j]) && isprint(p[j]))
					printf("%c", p[j]);
				else
					printf(".");
			}
		}
		printf("\n");
	}
}

static void
uk_netdev_queue_intr(struct uk_netdev *dev,
    uint16_t queue_id __unused, void *__unused)
{
	struct uk_netbuf *nb;
	int r;

	do {
		r = uk_netdev_rx_one(dev, 0, &nb);
		if (unlikely(r < 0))
			errx("uk_netdev_rx_one");
		if (uk_netdev_status_notready(r))
			break;
		dump_data(nb->data, nb->len);
		printf("\n");
		uk_netbuf_free(nb);
	} while (uk_netdev_status_more(r));
}

static uint16_t
dev_alloc_rxpkts(void *argp, struct uk_netbuf *nb[], uint16_t count)
{
	struct uk_alloc *a;
	struct uk_netdev_info *dev_info;
	struct uk_netbuf *b;
	int i;

	a = uk_alloc_get_default();
	dev_info = argp;

	for (i = 0; i < count; ++i) {
		b = uk_netbuf_alloc_buf(a, PKT_BUFLEN, dev_info->ioalign,
		    dev_info->nb_encap_rx, 0, NULL);
		if (b == NULL) {
			nb[i] = NULL;
			break;
		}
		b->len = b->buflen - dev_info->nb_encap_rx;
		nb[i] = b;
	}

	printf("%s: %d packets (why twice on boot?)\n", __func__, i);

	return (i);
}

int
main(int __unused, char *__unused[])
{
	struct uk_alloc *a;
	struct uk_netdev *dev;
	struct uk_netdev_conf dev_conf;
	struct uk_netdev_info dev_info = {0};
	struct uk_netdev_rxqueue_conf rxq_conf;
	struct uk_netdev_txqueue_conf txq_conf = {0};
	const struct uk_hwaddr *hwaddr;

	printf("herrow herrow\n");

	a = uk_alloc_get_default();
	if (a == NULL)
		errx("uk_alloc_get_default");

	dev = uk_netdev_get(0);
	if (dev == NULL)
		errx("uk_netdev_get");
	/* make sure no one probed before us */
	if (uk_netdev_state_get(dev) != UK_NETDEV_UNPROBED)
		errx("uk_netdev_state_get not UNPROBED");
	if (uk_netdev_probe(dev) != 0)
		errx("uk_netdev_probe");
	if (uk_netdev_state_get(dev) != UK_NETDEV_UNCONFIGURED)
		errx("uk_netdev_state_get not UNCONFIGURED");
	/* Get device information */
	uk_netdev_info_get(dev, &dev_info);	    /* void */
	/* Are we polling? */
#ifndef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
	dev_info.features &= ~UK_NETDEV_F_RXQ_INTR;
#endif /* !CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */
	/* Queue configuration, one pair */
	dev_conf.nb_rx_queues = 1;
	dev_conf.nb_tx_queues = 1;
	if (uk_netdev_configure(dev, &dev_conf) != 0)
		errx("uk_netdev_configure");
	/* RX */
	rxq_conf.a = a;
	rxq_conf.alloc_rxpkts = dev_alloc_rxpkts;
	rxq_conf.alloc_rxpkts_argp = &dev_info;
	/* We are polling so no callbacks */
#ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
	rxq_conf.callback = uk_netdev_queue_intr;
	rxq_conf.callback_cookie = NULL;
	rxq_conf.s = uk_sched_current();
#else
	rxq_conf.callback = NULL;
	rxq_conf.callback_cookie = NULL;
#endif	/* CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */

	if (uk_netdev_rxq_configure(dev, 0, 0, &rxq_conf) != 0)
		errx("uk_netdev_rxq_configure");
	/* TX */
	txq_conf.a = a;
	if (uk_netdev_txq_configure(dev, 0, 0, &txq_conf) != 0)
		errx("uk_netdev_txq_configure");
	/* Kick it */
	if (uk_netdev_start(dev) != 0)
		errx("uk_netdev_start");
	/* print mac */
	UK_ASSERT(UK_NETDEV_HWADDR_LEN == 6);
	if ((hwaddr = uk_netdev_hwaddr_get(dev)) == NULL)
		errx("uk_netdev_hwaddr_get");
	printf("hwaddr=%02x:%02x:%02x:%02x:%02x:%02x\n",
	    hwaddr->addr_bytes[0], hwaddr->addr_bytes[1],
	    hwaddr->addr_bytes[2], hwaddr->addr_bytes[3],
	    hwaddr->addr_bytes[4], hwaddr->addr_bytes[5]);

#ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
	switch (uk_netdev_rxq_intr_enable(dev, 0)) {
	case 0:
		break;
	case 1: /* device requested us to poll/flush the rings */
		uk_netdev_queue_intr(dev, 0, NULL);
		break;
	default:
		errx("uk_netdev_rxq_intr_enable");
	}
	for (;;)
		millisleep(1000);
#else /* CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */
	for (;;) {
		uk_netdev_queue_intr(dev, 0, NULL);
		millisleep(10);
	}
#endif	/* CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */

	printf("bye bye\n");

	return (0);
}
