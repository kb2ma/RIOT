# name of your application
APPLICATION = wakaama

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# TinyDTLS only has support for 32-bit architectures ATM
BOARD_BLACKLIST := arduino-duemilanove arduino-mega2560 arduino-uno chronos \
                   msb-430 msb-430h telosb waspmote-pro wsn430-v1_3b wsn430-v1_4 \
                   z1

BOARD_INSUFFICIENT_MEMORY := airfy-beacon b-l072z-lrwan1 bluepill calliope-mini \
                             cc2650-launchpad cc2650stk maple-mini \
                             microbit nrf51dongle nrf6310 nucleo32-f031 \
                             nucleo32-f042 nucleo32-f303 nucleo32-l031 nucleo-f030 \
                             nucleo-f070 nucleo-f072 nucleo-f103 nucleo-f302 nucleo-f334 \
                             nucleo-l053 nucleo-l073 opencm904 \
                             spark-core stm32f0discovery yunjia-nrf51822


# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
# Specify the mandatory networking modules
USEMODULE += gnrc_ipv6_router_default
USEMODULE += gnrc_sock_udp
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Specific the server URI  address (NOTE: Domain names not supported yet)
SERVER_URI ?= '"coap://[fd00:dead:beef::1]"'

ifneq (,$(SERVER_URI))
  CFLAGS += -DLWM2M_SERVER_URI=$(SERVER_URI)
endif

# NOTE: Add the package for wakaama
USEPKG += wakaama
# Uncomment to enable Wakaama debug log
#CFLAGS += -DLWM2M_WITH_LOGS

# NOTE: Use wakaama in client mode with bootstrapping enabled
CFLAGS += -DLWM2M_CLIENT_MODE -DLWM2M_BOOTSTRAP

include $(RIOTBASE)/Makefile.include
