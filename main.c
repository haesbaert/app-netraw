#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <uk/essentials.h>
#include <uk/netdev.h>

#ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
#error not ready for CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
#endif

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

#define PKT_BUFLEN 2048

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

	printf("%s: %d packets (why twice on boot??)\n", __func__, i);

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
		errx("uk_netdev_state_get not UNPROBED (%d)",
		    uk_netdev_state_get(dev));
	if (uk_netdev_probe(dev) != 0)
		errx("uk_netdev_probe");
	/* XXX at this point lwip/uknetdev does a bunch of eget_info */
	/* netdev has to be in unconfigured state */
	if (uk_netdev_state_get(dev) != UK_NETDEV_UNCONFIGURED)
		errx("uk_netdev_state_get not UNCONFIGURED");
	/* Get device information */
	uk_netdev_info_get(dev, &dev_info);	    /* void */
	/* We're gonna poll */
	dev_info.features &= ~UK_NETDEV_F_RXQ_INTR; /* XXX are we? */
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
	rxq_conf.callback = NULL;
	rxq_conf.callback_cookie = NULL;
	/* XXX polling so don't want dispatcher threads */
/* #ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */
/* 	rxq_conf.s = uk_sched_current(); */
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

	printf("bye bye\n");

	return (0);
}
