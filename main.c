#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <uk/essentials.h>
#include <uk/netdev.h>

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

static uint16_t
dev_alloc_rxpkts(void *argp, struct uk_netbuf *nb[], uint16_t count)
{
	/* XXX TODO XXX */
	argp=argp;nb=nb;count=count;

	return (0);
}


int
main(int __unused, char *__unused[])
{
	struct uk_alloc *a;
	struct uk_netdev *dev;
	struct uk_netdev_conf dev_conf;
	struct uk_netdev_info dev_info = {0};
	struct uk_netdev_rxqueue_conf rxq_conf;
	/* struct uk_netdev_txqueue_conf txq_conf = {0}; */

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
	/* netdev has to be in unconfigured state */
	if (uk_netdev_state_get(dev) != UK_NETDEV_UNCONFIGURED)
		errx("uk_netdev_state_get not UNCONFIGURED (%d)", uk_netdev_state_get(dev));
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
	rxq_conf.alloc_rxpkts_argp = NULL;
	/* We are polling so no callbacks */
	rxq_conf.callback = NULL;
	rxq_conf.callback_cookie = NULL;
	/* XXX polling so don't want dispatcher threads */
/* #ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */
/* 	rxq_conf.s = uk_sched_current(); */
	if (uk_netdev_rxq_configure(dev, 0, 0, &rxq_conf) != 0)
		errx("uk_netdev_rxq_configure");

	printf("bye bye \n");

	return (0);
}
