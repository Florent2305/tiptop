<?php
$domain=ereg_replace('[^\.]*\.(.*)$','\1',$_SERVER['HTTP_HOST']);
$group_name=ereg_replace('([^\.]*)\..*$','\1',$_SERVER['HTTP_HOST']);
echo '<?xml version="1.0" encoding="UTF-8"?>';
?>
<!DOCTYPE html
	PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">

<head>
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<title>Tiptop</title>
<!--
<script language="JavaScript" type="text/javascript">
function help_window(helpurl) {
  HelpWin = window.open( helpurl,'HelpWindow','scrollbars=yes,resizable=yes,toolbar=no,height=400,width=400');
}
</script>
// -->
<style type="text/css">
BODY {
  margin-top: 3;
  margin-left: 3;
  margin-right: 3;
  margin-bottom: 3;
  background: #ffffff;
}
ol,ul,p,body,td,tr,th,form {
  font-family: verdana,arial,helvetica,sans-serif; font-size:small;
  color: #333333;
}

h1 { font-size: x-large; font-family: verdana,arial,helvetica,sans-serif; }
h2 { font-size: large; font-family: verdana,arial,helvetica,sans-serif; }
h3 { font-size: medium; font-family: verdana,arial,helvetica,sans-serif; }
h4 { font-size: small; font-family: verdana,arial,helvetica,sans-serif; }
h5 { font-size: x-small; font-family: verdana,arial,helvetica,sans-serif; }
h6 { font-size: xx-small; font-family: verdana,arial,helvetica,sans-serif; }

pre,tt { font-family: courier,sans-serif }

a:link { text-decoration:none }
a:visited { text-decoration:none }
a:active { text-decoration:none }
a:hover { text-decoration:underline; color:red }

.titlebar { color: black; text-decoration: none; font-weight: bold; }
a.tablink { color: black; text-decoration: none; font-weight: bold; font-size: x-small; }
a.tablink:visited { color: black; text-decoration: none; font-weight: bold; font-size: x-small; }
a.tablink:hover { text-decoration: none; color: black; font-weight: bold; font-size: x-small; }
a.tabsellink { color: black; text-decoration: none; font-weight: bold; font-size: x-small; }
a.tabsellink:visited { color: black; text-decoration: none; font-weight: bold; font-size: x-small; }
a.tabsellink:hover { text-decoration: none; color: black; font-weight: bold; font-size: x-small; }
</style>
</head>

<body>
<h1>Tiptop</h1>

<h2>Motivation</h2>

<div style="float:right; padding:10px"><a href="tiptop-snapshot.gif"><img src="tiptop-snapshot.gif"
border="0" width="400" height="269" style="border: 0;" /></a></div>

<p>Hardware performance monitoring counters have recently received a
lot of attention. They have been used by diverse communities to
understand and improve the quality of computing systems: for example,
architects use them to extract application characteristics and propose
new hardware mechanisms; compiler writers study how generated code
behaves on particular hardware; software developers identify critical
regions of their applications and evaluate design choices to select
the best performing implementation. We propose that counters be used
by all categories of users, in particular non-experts, and we advocate
that a few simple metrics derived from these counters are relevant and
useful. For example, a low IPC (number of executed instructions per
cycle) indicates that the hardware is not performing at its best; a
high cache miss ratio can suggest several causes, such as conflicts
between processes in a multicore environment.</p>

<h2>Citing</h2>
If you like tiptop and use it in your research, please reference the
technical report Inria RR-7789: <a href="http://hal.inria.fr/hal-00639173">http://hal.inria.fr/hal-00639173</a>.
<div style="margin:0.5cm">
  Tiptop: Hardware Performance Counters for the Masses, Erven Rohou,<br>
  Inria Research Report 7789, Nov 2011.
</div>


<div style="float:right; margin:10px;border:solid black 1px;clear:right;font-size:75%">
<pre>
&lt;tiptop>

  &lt;options>
    &lt;option name="cpu_threshold" value="0.00001"/>
    &lt;option name="delay" value="2.000000" />
    &lt;option name="batch" value="0" />
  &lt;/options>

  &lt;screen name="default" desc="Screen by default">
    &lt;counter alias="CYCLE" config="CPU_CYCLES" />
    &lt;counter alias="INSN" config="INSTRUCTIONS" />
    &lt;counter alias="MISS" config="CACHE_MISSES" />
    &lt;counter alias="BR" config="BRANCH_MISSES" />

    &lt;column header=" %CPU" format="%5.1f"
        desc="(unknown)" expr="CPU_TOT" />
    &lt;column header="   P" format="  %2.0f" 
        desc="(unknown)" expr="PROC_ID" />
    &lt;column header="  Mcycle" format="%8.2f"
        desc="Cycles (millions)"
        expr="delta(CYCLE)/1000000"/>
    &lt;column header="  Minstr" format="%8.2f"
        desc="Instructions (millions)"
        expr="delta(INSN)/1000000"/>
    &lt;column header=" IPC" format="%4.2f"
        desc="Executed instructions per cycle"
        expr="delta(INSN) / delta(CYCLE)" />
    &lt;column header=" %MISS" format="%6.2f"
        desc="Cache miss per instruction"
        expr="100 * (delta(MISS) / delta(INSN))" />
    &lt;column header=" %BMIS" format="%6.2f"
        desc="Branch misprediction per 100 instructions"
        expr="100 * delta(BR) / delta(INSN)" />
  &lt;/screen>
&lt;/tiptop>
</pre></div>

<h2>Features</h2>
<ul>
<li>No root privilege needed</li>
<li>No patch to OS</li>
<li>No module to load</li>
<li>No need to instrument applications</li>
<li>No need to even restart applications</li>
<li>Any event supported by the hardware
<ul>
<li>some predefined: instructions, cycles, LLC misses (easy)</li>
<li>any hardware supported event (slightly harder)</li>
</ul>
</li>
<li>Live mode and batch mode</li>
<li>Configure file to define counters and screens</li>
<li>Pretty much like <em>top</em></li>
</ul>

<h2>Requirements</h2>
<ul>
<li>Linux 2.6.31+</li>
<li>Nice to have: libcurses and libxml2</li>
</ul>


<h2>Download</h2>
<p>Tiptop is available in source code on the Inria
forge: <a href="https://gforge.inria.fr/projects/tiptop" title="Tiptop
on Inria forge">https://gforge.inria.fr/projects/tiptop</a>.</p>
<ul>
<li>Latest release: <a href="releases/tiptop-2.3.1.tar.gz">tiptop-2.3.1.tar.gz</a></li>
<li>Previous releases:
<ul>
  <li><a href="releases/tiptop-2.3.tar.gz">tiptop-2.3.tar.gz</a></li>
  <li><a href="releases/tiptop-2.2.tar.gz">tiptop-2.2.tar.gz</a></li>
  <li><a href="releases/tiptop-2.1.tar.gz">tiptop-2.1.tar.gz</a></li>
  <li><a href="releases/tiptop-2.0.tar.gz">tiptop-2.0.tar.gz</a></li>
  <li><a href="releases/tiptop-1.0.1.tar.gz">tiptop-1.0.1.tar.gz</a></li>
  <li><a href="releases/tiptop-1.0.tar.gz">tiptop-1.0.tar.gz</a></li>
</ul>
</ul>

<h2>More Information</h2>
<p>More information on tiptop is available in our Research
Report <a href="http://hal.inria.fr/hal-00639173/" title="Tiptop:
Hardware Performance Counters for the Masses">RR-7789</a>.</p>

<p>Be sure to check the README and the man page included in the distribution.</p>

<p>Specific counters for Intel architectures can be found in the
<a href="http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html">Intel64 and IA-32 Architectures Software Developer's Manual</a>, Volume
3B.</p>


<h2>Credits and Legal</h2>
<p>Tiptop is proposed by the <a href="https://team.inria.fr/pacap/">PACAP
project team</a>
at <a href="http://www.inria.fr/en/centre/rennes">Inria Rennes
Bretagne Atlantique</a>. The main developer
is <a href="https://team.inria.fr/pacap/members/rohou/">Erven Rohou</a>.</p>
<p>Tiptop is copyright Inria and released under GPL v2.<br/> It is
registered at the APP (Agence de Protection des Programmes) under
number IDDN.FR.001.450006.000.S.P.2011.000.10800.</p>


<hr/>
<!-- PLEASE LEAVE "Powered By GForge" on your site -->
<br />
<center>
<a href="http://gforge.org/"><img src="http://<?php echo $domain; ?>/images/pow-fusionforge.png" alt="Powered By GForge Collaborative Development Environment" border="0" /></a>
</center>


</body>
</html>
