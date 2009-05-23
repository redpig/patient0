##
# $Id: $
##

##
# This file is part of the Metasploit Framework and may be subject to 
# redistribution and commercial restrictions. Please see the Metasploit
# Framework web site for more information on licensing and terms of use.
# http://metasploit.com/framework/
##


require 'msf/core'
require 'msf/core/payload/osx/patient0'


###
#
# Injects the patient0 syringe bundle and a given pathogen.
#
###
module Metasploit3

	include Msf::Payload::Osx::Patient0

end
