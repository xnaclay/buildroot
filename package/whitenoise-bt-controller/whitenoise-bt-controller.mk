################################################################################
#
# whitenoise-bt-controller
#
################################################################################

WHITENOISE_BT_CONTROLLER_VERSION = 1.0
WHITENOISE_BT_CONTROLLER_SITE = whitenoise-bt-controller
WHITENOISE_BT_CONTROLLER_SITE_METHOD = local
WHITENOISE_BT_CONTROLLER_DEPENDENCIES = qt5connectivity

$(eval $(cmake-package))
