# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#      Stefan de Konink <stefan@konink.de>
#
# Copyright (C) 2001-2013 Alvaro Lopez Ortega
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of version 2 of the GNU General Public
# License as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import CTK
import Handler
import Cherokee
import Balancer

from util import *
from consts import *

URL_APPLY = '/plugin/geocoder/apply'
NOTE_INDICES = N_("Indices to be used when searching.")

class Plugin_geocoder (Handler.PluginHandler):
    def __init__ (self, key, **kwargs):
        kwargs['show_document_root'] = False
        Handler.PluginHandler.__init__ (self, key, **kwargs)
        Handler.PluginHandler.AddCommon (self)

        table = CTK.PropsTable()
        table.Add (_('Indices'),   CTK.TextCfg('%s!indices'%(key), False),  _(NOTE_INDICES))

        submit = CTK.Submitter (URL_APPLY)
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Geocodec Configuration')))
        self += CTK.Indenter (submit)

        # Load Balancing
        modul = CTK.PluginSelector('%s!balancer'%(key), trans_options(Cherokee.support.filter_available (BALANCERS)))
        table = CTK.PropsTable()
        table.Add (_("Balancer"), modul.selector_widget, _(Balancer.NOTE_BALANCER))


        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Sphinx Balancing')))
        self += CTK.Indenter (table)
        self += modul

CTK.publish ('^%s'%(URL_APPLY), CTK.cfg_apply_post, method="POST")
