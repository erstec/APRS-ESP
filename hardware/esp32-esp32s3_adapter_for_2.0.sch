<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="9.6.2">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="no" active="no"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="no" active="no"/>
<layer number="17" name="Pads" color="2" fill="1" visible="no" active="no"/>
<layer number="18" name="Vias" color="2" fill="1" visible="no" active="no"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="no" active="no"/>
<layer number="20" name="Dimension" color="15" fill="1" visible="no" active="no"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="23" name="tOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="24" name="bOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="25" name="tNames" color="7" fill="1" visible="no" active="no"/>
<layer number="26" name="bNames" color="7" fill="1" visible="no" active="no"/>
<layer number="27" name="tValues" color="7" fill="1" visible="no" active="no"/>
<layer number="28" name="bValues" color="7" fill="1" visible="no" active="no"/>
<layer number="29" name="tStop" color="7" fill="3" visible="no" active="no"/>
<layer number="30" name="bStop" color="7" fill="6" visible="no" active="no"/>
<layer number="31" name="tCream" color="7" fill="4" visible="no" active="no"/>
<layer number="32" name="bCream" color="7" fill="5" visible="no" active="no"/>
<layer number="33" name="tFinish" color="6" fill="3" visible="no" active="no"/>
<layer number="34" name="bFinish" color="6" fill="6" visible="no" active="no"/>
<layer number="35" name="tGlue" color="7" fill="4" visible="no" active="no"/>
<layer number="36" name="bGlue" color="7" fill="5" visible="no" active="no"/>
<layer number="37" name="tTest" color="7" fill="1" visible="no" active="no"/>
<layer number="38" name="bTest" color="7" fill="1" visible="no" active="no"/>
<layer number="39" name="tKeepout" color="4" fill="11" visible="no" active="no"/>
<layer number="40" name="bKeepout" color="1" fill="11" visible="no" active="no"/>
<layer number="41" name="tRestrict" color="4" fill="10" visible="no" active="no"/>
<layer number="42" name="bRestrict" color="1" fill="10" visible="no" active="no"/>
<layer number="43" name="vRestrict" color="2" fill="10" visible="no" active="no"/>
<layer number="44" name="Drills" color="7" fill="1" visible="no" active="no"/>
<layer number="45" name="Holes" color="7" fill="1" visible="no" active="no"/>
<layer number="46" name="Milling" color="3" fill="1" visible="no" active="no"/>
<layer number="47" name="Measures" color="7" fill="1" visible="no" active="no"/>
<layer number="48" name="Document" color="7" fill="1" visible="no" active="no"/>
<layer number="49" name="Reference" color="7" fill="1" visible="no" active="no"/>
<layer number="51" name="tDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="52" name="bDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="88" name="SimResults" color="9" fill="1" visible="yes" active="yes"/>
<layer number="89" name="SimProbes" color="9" fill="1" visible="yes" active="yes"/>
<layer number="90" name="Modules" color="5" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="ESP32-S3-WROOM-1-N16R2">
<packages>
<package name="XCVR_ESP32-S3-WROOM-1-N16R2">
<wire x1="-9" y1="12.75" x2="9" y2="12.75" width="0.127" layer="51"/>
<wire x1="9" y1="12.75" x2="9" y2="6.75" width="0.127" layer="51"/>
<wire x1="9" y1="6.75" x2="9" y2="-12.75" width="0.127" layer="51"/>
<wire x1="9" y1="-12.75" x2="-9" y2="-12.75" width="0.127" layer="51"/>
<wire x1="-9" y1="-12.75" x2="-9" y2="6.75" width="0.127" layer="51"/>
<wire x1="-9" y1="6.75" x2="-9" y2="12.75" width="0.127" layer="51"/>
<wire x1="-9.75" y1="-13.5" x2="-9.75" y2="13" width="0.05" layer="40"/>
<wire x1="-9.75" y1="13" x2="9.75" y2="13" width="0.05" layer="40"/>
<wire x1="9.75" y1="13" x2="9.75" y2="-13.5" width="0.05" layer="40"/>
<wire x1="-9.75" y1="-13.5" x2="9.75" y2="-13.5" width="0.05" layer="40"/>
<circle x="-10.2" y="5.26" radius="0.1" width="0.2" layer="51"/>
<wire x1="-9" y1="12.75" x2="9" y2="12.75" width="0.127" layer="22"/>
<wire x1="9" y1="12.75" x2="9" y2="6.03" width="0.127" layer="22"/>
<wire x1="-9" y1="6.03" x2="-9" y2="12.75" width="0.127" layer="22"/>
<circle x="-10.2" y="5.26" radius="0.1" width="0.2" layer="21"/>
<wire x1="-9" y1="-12.02" x2="-9" y2="-12.75" width="0.127" layer="21"/>
<wire x1="-9" y1="-12.75" x2="-7.755" y2="-12.75" width="0.127" layer="22"/>
<wire x1="9" y1="-12.02" x2="9" y2="-12.75" width="0.127" layer="21"/>
<wire x1="9" y1="-12.75" x2="7.755" y2="-12.75" width="0.127" layer="21"/>
<smd name="41_1" x="-1.5" y="-2.46" dx="0.9" dy="0.9" layer="16"/>
<smd name="1" x="-8.75" y="5.26" dx="1.5" dy="0.9" layer="16"/>
<smd name="41_2" x="-2.9" y="-2.46" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_3" x="-0.1" y="-2.46" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_4" x="-2.9" y="-3.86" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_5" x="-1.5" y="-3.86" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_6" x="-0.1" y="-3.86" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_7" x="-2.9" y="-1.06" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_8" x="-1.5" y="-1.06" dx="0.9" dy="0.9" layer="16"/>
<smd name="41_9" x="-0.1" y="-1.06" dx="0.9" dy="0.9" layer="16"/>
<smd name="2" x="-8.75" y="3.99" dx="1.5" dy="0.9" layer="16"/>
<smd name="3" x="-8.75" y="2.72" dx="1.5" dy="0.9" layer="16"/>
<smd name="4" x="-8.75" y="1.45" dx="1.5" dy="0.9" layer="16"/>
<smd name="5" x="-8.75" y="0.18" dx="1.5" dy="0.9" layer="16"/>
<smd name="6" x="-8.75" y="-1.09" dx="1.5" dy="0.9" layer="16"/>
<smd name="7" x="-8.75" y="-2.36" dx="1.5" dy="0.9" layer="16"/>
<smd name="8" x="-8.75" y="-3.63" dx="1.5" dy="0.9" layer="16"/>
<smd name="9" x="-8.75" y="-4.9" dx="1.5" dy="0.9" layer="16"/>
<smd name="10" x="-8.75" y="-6.17" dx="1.5" dy="0.9" layer="16"/>
<smd name="11" x="-8.75" y="-7.44" dx="1.5" dy="0.9" layer="16"/>
<smd name="12" x="-8.75" y="-8.71" dx="1.5" dy="0.9" layer="16"/>
<smd name="13" x="-8.75" y="-9.98" dx="1.5" dy="0.9" layer="16"/>
<smd name="14" x="-8.75" y="-11.25" dx="1.5" dy="0.9" layer="16"/>
<smd name="15" x="-6.985" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="27" x="8.75" y="-11.25" dx="1.5" dy="0.9" layer="16"/>
<smd name="28" x="8.75" y="-9.98" dx="1.5" dy="0.9" layer="16"/>
<smd name="29" x="8.75" y="-8.71" dx="1.5" dy="0.9" layer="16"/>
<smd name="30" x="8.75" y="-7.44" dx="1.5" dy="0.9" layer="16"/>
<smd name="31" x="8.75" y="-6.17" dx="1.5" dy="0.9" layer="16"/>
<smd name="32" x="8.75" y="-4.9" dx="1.5" dy="0.9" layer="16"/>
<smd name="33" x="8.75" y="-3.63" dx="1.5" dy="0.9" layer="16"/>
<smd name="34" x="8.75" y="-2.36" dx="1.5" dy="0.9" layer="16"/>
<smd name="35" x="8.75" y="-1.09" dx="1.5" dy="0.9" layer="16"/>
<smd name="36" x="8.75" y="0.18" dx="1.5" dy="0.9" layer="16"/>
<smd name="37" x="8.75" y="1.45" dx="1.5" dy="0.9" layer="16"/>
<smd name="38" x="8.75" y="2.72" dx="1.5" dy="0.9" layer="16"/>
<smd name="39" x="8.75" y="3.99" dx="1.5" dy="0.9" layer="16"/>
<smd name="40" x="8.75" y="5.26" dx="1.5" dy="0.9" layer="16"/>
<smd name="16" x="-5.715" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="17" x="-4.445" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="18" x="-3.175" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="19" x="-1.905" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="20" x="-0.635" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="21" x="0.635" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="22" x="1.905" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="23" x="3.175" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="24" x="4.445" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="25" x="5.715" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
<smd name="26" x="6.985" y="-12.5" dx="0.9" dy="1.5" layer="16"/>
</package>
</packages>
<symbols>
<symbol name="ESP32-S3-WROOM-1-N16R2">
<wire x1="-10.16" y1="33.02" x2="10.16" y2="33.02" width="0.254" layer="94"/>
<wire x1="10.16" y1="33.02" x2="10.16" y2="-33.02" width="0.254" layer="94"/>
<wire x1="10.16" y1="-33.02" x2="-10.16" y2="-33.02" width="0.254" layer="94"/>
<wire x1="-10.16" y1="-33.02" x2="-10.16" y2="33.02" width="0.254" layer="94"/>
<text x="-10.16" y="34.1122" size="1.778" layer="95">&gt;NAME</text>
<text x="-10.16" y="-35.56" size="1.778" layer="96">&gt;VALUE</text>
<pin name="GND" x="15.24" y="-30.48" length="middle" direction="pwr" rot="R180"/>
<pin name="3V3" x="15.24" y="30.48" length="middle" direction="pwr" rot="R180"/>
<pin name="EN" x="-15.24" y="27.94" length="middle" direction="in"/>
<pin name="IO35" x="15.24" y="2.54" length="middle" rot="R180"/>
<pin name="IO41" x="15.24" y="-12.7" length="middle" rot="R180"/>
<pin name="IO39" x="15.24" y="-7.62" length="middle" rot="R180"/>
<pin name="IO40" x="15.24" y="-10.16" length="middle" rot="R180"/>
<pin name="IO14" x="-15.24" y="-20.32" length="middle"/>
<pin name="IO12" x="-15.24" y="-15.24" length="middle"/>
<pin name="IO13" x="-15.24" y="-17.78" length="middle"/>
<pin name="IO15" x="-15.24" y="-22.86" length="middle"/>
<pin name="IO2" x="-15.24" y="10.16" length="middle"/>
<pin name="IO0" x="-15.24" y="15.24" length="middle"/>
<pin name="IO4" x="-15.24" y="5.08" length="middle"/>
<pin name="IO16" x="-15.24" y="-25.4" length="middle"/>
<pin name="IO17" x="15.24" y="15.24" length="middle" rot="R180"/>
<pin name="IO5" x="-15.24" y="2.54" length="middle"/>
<pin name="IO18" x="15.24" y="12.7" length="middle" rot="R180"/>
<pin name="IO19" x="15.24" y="10.16" length="middle" rot="R180"/>
<pin name="IO21" x="15.24" y="5.08" length="middle" rot="R180"/>
<pin name="IO37" x="15.24" y="-2.54" length="middle" rot="R180"/>
<pin name="IO38" x="15.24" y="-5.08" length="middle" rot="R180"/>
<pin name="IO1" x="-15.24" y="12.7" length="middle"/>
<pin name="IO3" x="-15.24" y="7.62" length="middle"/>
<pin name="IO6" x="-15.24" y="0" length="middle"/>
<pin name="IO7" x="-15.24" y="-2.54" length="middle"/>
<pin name="IO8" x="-15.24" y="-5.08" length="middle"/>
<pin name="IO9" x="-15.24" y="-7.62" length="middle"/>
<pin name="IO10" x="-15.24" y="-10.16" length="middle"/>
<pin name="IO11" x="-15.24" y="-12.7" length="middle"/>
<pin name="IO36" x="15.24" y="0" length="middle" rot="R180"/>
<pin name="IO42" x="15.24" y="-15.24" length="middle" rot="R180"/>
<pin name="IO20" x="15.24" y="7.62" length="middle" rot="R180"/>
<pin name="TXD0" x="-15.24" y="20.32" length="middle"/>
<pin name="RXD0" x="-15.24" y="22.86" length="middle"/>
<pin name="IO45" x="15.24" y="-17.78" length="middle" rot="R180"/>
<pin name="IO46" x="15.24" y="-20.32" length="middle" rot="R180"/>
<pin name="IO47" x="15.24" y="-22.86" length="middle" rot="R180"/>
<pin name="IO48" x="15.24" y="-25.4" length="middle" rot="R180"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="ESP32-S3-WROOM-1-N16R2" prefix="U">
<description> &lt;a href="https://pricing.snapeda.com/parts/ESP32-S3-WROOM-1-N16R2/Espressif%20Systems/view-part?ref=eda"&gt;Check availability&lt;/a&gt;</description>
<gates>
<gate name="G$1" symbol="ESP32-S3-WROOM-1-N16R2" x="0" y="0"/>
</gates>
<devices>
<device name="" package="XCVR_ESP32-S3-WROOM-1-N16R2">
<connects>
<connect gate="G$1" pin="3V3" pad="2"/>
<connect gate="G$1" pin="EN" pad="3"/>
<connect gate="G$1" pin="GND" pad="1 40 41_1 41_2 41_3 41_4 41_5 41_6 41_7 41_8 41_9"/>
<connect gate="G$1" pin="IO0" pad="27"/>
<connect gate="G$1" pin="IO1" pad="39"/>
<connect gate="G$1" pin="IO10" pad="18"/>
<connect gate="G$1" pin="IO11" pad="19"/>
<connect gate="G$1" pin="IO12" pad="20"/>
<connect gate="G$1" pin="IO13" pad="21"/>
<connect gate="G$1" pin="IO14" pad="22"/>
<connect gate="G$1" pin="IO15" pad="8"/>
<connect gate="G$1" pin="IO16" pad="9"/>
<connect gate="G$1" pin="IO17" pad="10"/>
<connect gate="G$1" pin="IO18" pad="11"/>
<connect gate="G$1" pin="IO19" pad="13"/>
<connect gate="G$1" pin="IO2" pad="38"/>
<connect gate="G$1" pin="IO20" pad="14"/>
<connect gate="G$1" pin="IO21" pad="23"/>
<connect gate="G$1" pin="IO3" pad="15"/>
<connect gate="G$1" pin="IO35" pad="28"/>
<connect gate="G$1" pin="IO36" pad="29"/>
<connect gate="G$1" pin="IO37" pad="30"/>
<connect gate="G$1" pin="IO38" pad="31"/>
<connect gate="G$1" pin="IO39" pad="32"/>
<connect gate="G$1" pin="IO4" pad="4"/>
<connect gate="G$1" pin="IO40" pad="33"/>
<connect gate="G$1" pin="IO41" pad="34"/>
<connect gate="G$1" pin="IO42" pad="35"/>
<connect gate="G$1" pin="IO45" pad="26"/>
<connect gate="G$1" pin="IO46" pad="16"/>
<connect gate="G$1" pin="IO47" pad="24"/>
<connect gate="G$1" pin="IO48" pad="25"/>
<connect gate="G$1" pin="IO5" pad="5"/>
<connect gate="G$1" pin="IO6" pad="6"/>
<connect gate="G$1" pin="IO7" pad="7"/>
<connect gate="G$1" pin="IO8" pad="12"/>
<connect gate="G$1" pin="IO9" pad="17"/>
<connect gate="G$1" pin="RXD0" pad="36"/>
<connect gate="G$1" pin="TXD0" pad="37"/>
</connects>
<technologies>
<technology name="">
<attribute name="AVAILABILITY" value="In Stock"/>
<attribute name="DESCRIPTION" value=" Bluetooth, WiFi 802.11b/g/n, Bluetooth v5.0 Transceiver Module 2.4GHz PCB Trace Surface Mount "/>
<attribute name="MF" value="Espressif Systems"/>
<attribute name="MP" value="ESP32-S3-WROOM-1-N16R2"/>
<attribute name="PACKAGE" value="None"/>
<attribute name="PRICE" value="None"/>
<attribute name="PURCHASE-URL" value="https://pricing.snapeda.com/search?q=ESP32-S3-WROOM-1-N16R2&amp;ref=eda"/>
</technology>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
<library name="ESP32-WROOM-32E__16MB_">
<packages>
<package name="XCVR_ESP32-WROOM-32E_(16MB)">
<wire x1="-9" y1="12.75" x2="9" y2="12.75" width="0.127" layer="51"/>
<wire x1="9" y1="12.75" x2="9" y2="6.56" width="0.127" layer="51"/>
<wire x1="9" y1="6.56" x2="9" y2="-12.75" width="0.127" layer="51"/>
<wire x1="9" y1="-12.75" x2="-9" y2="-12.75" width="0.127" layer="51"/>
<wire x1="-9" y1="-12.75" x2="-9" y2="6.56" width="0.127" layer="51"/>
<wire x1="-9" y1="12.75" x2="-9" y2="6.56" width="0.127" layer="51"/>
<wire x1="-9" y1="6.56" x2="9" y2="6.56" width="0.127" layer="51"/>
<wire x1="-9.25" y1="13" x2="9.25" y2="13" width="0.05" layer="39"/>
<wire x1="9.25" y1="13" x2="9.25" y2="5.96" width="0.05" layer="39"/>
<wire x1="9.25" y1="5.96" x2="9.75" y2="5.96" width="0.05" layer="39"/>
<wire x1="9.75" y1="5.96" x2="9.75" y2="-13.5" width="0.05" layer="39"/>
<wire x1="9.75" y1="-13.5" x2="-9.75" y2="-13.5" width="0.05" layer="39"/>
<wire x1="-9.75" y1="-13.5" x2="-9.75" y2="5.96" width="0.05" layer="39"/>
<wire x1="-9.75" y1="5.96" x2="-9.25" y2="5.96" width="0.05" layer="39"/>
<wire x1="-9.25" y1="5.96" x2="-9.25" y2="13" width="0.05" layer="39"/>
<circle x="-10.2" y="5.26" radius="0.1" width="0.2" layer="51"/>
<circle x="-10.2" y="5.26" radius="0.1" width="0.2" layer="21"/>
<text x="-9.75" y="13.25" size="1.27" layer="25">&gt;NAME</text>
<text x="-9.75" y="-15" size="1.27" layer="27">&gt;VALUE</text>
<wire x1="-9" y1="-12.1" x2="-9" y2="-12.75" width="0.127" layer="21"/>
<wire x1="-9" y1="-12.75" x2="-6.55" y2="-12.75" width="0.127" layer="21"/>
<wire x1="6.55" y1="-12.75" x2="9" y2="-12.75" width="0.127" layer="21"/>
<wire x1="9" y1="-12.75" x2="9" y2="-12.1" width="0.127" layer="21"/>
<wire x1="-9" y1="6.25" x2="-9" y2="12.75" width="0.127" layer="21"/>
<wire x1="-9" y1="12.75" x2="9" y2="12.75" width="0.127" layer="21"/>
<wire x1="9" y1="12.75" x2="9" y2="6.25" width="0.127" layer="21"/>
<smd name="1" x="-8.75" y="5.26" dx="1.5" dy="0.9" layer="1"/>
<smd name="2" x="-8.75" y="3.99" dx="1.5" dy="0.9" layer="1"/>
<smd name="3" x="-8.75" y="2.72" dx="1.5" dy="0.9" layer="1"/>
<smd name="4" x="-8.75" y="1.45" dx="1.5" dy="0.9" layer="1"/>
<smd name="5" x="-8.75" y="0.18" dx="1.5" dy="0.9" layer="1"/>
<smd name="6" x="-8.75" y="-1.09" dx="1.5" dy="0.9" layer="1"/>
<smd name="7" x="-8.75" y="-2.36" dx="1.5" dy="0.9" layer="1"/>
<smd name="8" x="-8.75" y="-3.63" dx="1.5" dy="0.9" layer="1"/>
<smd name="9" x="-8.75" y="-4.9" dx="1.5" dy="0.9" layer="1"/>
<smd name="10" x="-8.75" y="-6.17" dx="1.5" dy="0.9" layer="1"/>
<smd name="11" x="-8.75" y="-7.44" dx="1.5" dy="0.9" layer="1"/>
<smd name="12" x="-8.75" y="-8.71" dx="1.5" dy="0.9" layer="1"/>
<smd name="13" x="-8.75" y="-9.98" dx="1.5" dy="0.9" layer="1"/>
<smd name="14" x="-8.75" y="-11.25" dx="1.5" dy="0.9" layer="1"/>
<smd name="15" x="-5.715" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="16" x="-4.445" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="17" x="-3.175" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="18" x="-1.905" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="19" x="-0.635" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="20" x="0.635" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="21" x="1.905" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="22" x="3.175" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="23" x="4.445" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="24" x="5.715" y="-12.5" dx="0.9" dy="1.5" layer="1"/>
<smd name="25" x="8.75" y="-11.25" dx="1.5" dy="0.9" layer="1"/>
<smd name="26" x="8.75" y="-9.98" dx="1.5" dy="0.9" layer="1"/>
<smd name="27" x="8.75" y="-8.71" dx="1.5" dy="0.9" layer="1"/>
<smd name="28" x="8.75" y="-7.44" dx="1.5" dy="0.9" layer="1"/>
<smd name="29" x="8.75" y="-6.17" dx="1.5" dy="0.9" layer="1"/>
<smd name="30" x="8.75" y="-4.9" dx="1.5" dy="0.9" layer="1"/>
<smd name="31" x="8.75" y="-3.63" dx="1.5" dy="0.9" layer="1"/>
<smd name="32" x="8.75" y="-2.36" dx="1.5" dy="0.9" layer="1"/>
<smd name="33" x="8.75" y="-1.09" dx="1.5" dy="0.9" layer="1"/>
<smd name="34" x="8.75" y="0.18" dx="1.5" dy="0.9" layer="1"/>
<smd name="35" x="8.75" y="1.45" dx="1.5" dy="0.9" layer="1"/>
<smd name="36" x="8.75" y="2.72" dx="1.5" dy="0.9" layer="1"/>
<smd name="37" x="8.75" y="3.99" dx="1.5" dy="0.9" layer="1"/>
<smd name="38" x="8.75" y="5.26" dx="1.5" dy="0.9" layer="1"/>
<smd name="39_1" x="-2.9" y="-1.06" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_2" x="-1.5" y="-1.06" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_3" x="-0.1" y="-1.06" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_4" x="-2.9" y="-2.46" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_5" x="-1.5" y="-2.46" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_6" x="-0.1" y="-2.46" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_7" x="-2.9" y="-3.86" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_8" x="-1.5" y="-3.86" dx="0.9" dy="0.9" layer="1"/>
<smd name="39_9" x="-0.1" y="-3.86" dx="0.9" dy="0.9" layer="1"/>
</package>
</packages>
<symbols>
<symbol name="ESP32-WROOM-32E_(16MB)">
<wire x1="-15.24" y1="33.02" x2="15.24" y2="33.02" width="0.254" layer="94"/>
<wire x1="15.24" y1="33.02" x2="15.24" y2="-30.48" width="0.254" layer="94"/>
<wire x1="15.24" y1="-30.48" x2="-15.24" y2="-30.48" width="0.254" layer="94"/>
<wire x1="-15.24" y1="-30.48" x2="-15.24" y2="33.02" width="0.254" layer="94"/>
<text x="-15.24" y="33.909" size="1.778" layer="95">&gt;NAME</text>
<text x="-15.24" y="-33.02" size="1.778" layer="96">&gt;VALUE</text>
<pin name="GND" x="20.32" y="-27.94" length="middle" direction="pwr" rot="R180"/>
<pin name="3V3" x="20.32" y="30.48" length="middle" direction="pwr" rot="R180"/>
<pin name="EN" x="-20.32" y="25.4" length="middle" direction="in"/>
<pin name="SENSOR_VP" x="-20.32" y="17.78" length="middle" direction="in"/>
<pin name="SENSOR_VN" x="-20.32" y="15.24" length="middle" direction="in"/>
<pin name="IO34" x="-20.32" y="7.62" length="middle" direction="in"/>
<pin name="IO35" x="-20.32" y="5.08" length="middle" direction="in"/>
<pin name="IO33" x="20.32" y="-22.86" length="middle" rot="R180"/>
<pin name="IO32" x="20.32" y="-20.32" length="middle" rot="R180"/>
<pin name="IO25" x="20.32" y="-12.7" length="middle" rot="R180"/>
<pin name="IO26" x="20.32" y="-15.24" length="middle" rot="R180"/>
<pin name="IO27" x="20.32" y="-17.78" length="middle" rot="R180"/>
<pin name="IO14" x="20.32" y="10.16" length="middle" rot="R180"/>
<pin name="IO12" x="20.32" y="15.24" length="middle" rot="R180"/>
<pin name="IO13" x="20.32" y="12.7" length="middle" rot="R180"/>
<pin name="IO15" x="20.32" y="7.62" length="middle" rot="R180"/>
<pin name="IO2" x="20.32" y="22.86" length="middle" rot="R180"/>
<pin name="IO0" x="20.32" y="25.4" length="middle" rot="R180"/>
<pin name="IO4" x="20.32" y="20.32" length="middle" rot="R180"/>
<pin name="IO16" x="20.32" y="5.08" length="middle" rot="R180"/>
<pin name="IO17" x="20.32" y="2.54" length="middle" rot="R180"/>
<pin name="IO5" x="20.32" y="17.78" length="middle" rot="R180"/>
<pin name="IO18" x="20.32" y="0" length="middle" rot="R180"/>
<pin name="IO19" x="20.32" y="-2.54" length="middle" rot="R180"/>
<pin name="IO21" x="20.32" y="-5.08" length="middle" rot="R180"/>
<pin name="IO22" x="20.32" y="-7.62" length="middle" rot="R180"/>
<pin name="IO23" x="20.32" y="-10.16" length="middle" rot="R180"/>
<pin name="RXD0" x="-20.32" y="-5.08" length="middle"/>
<pin name="TXD0" x="-20.32" y="-2.54" length="middle"/>
<pin name="NC" x="-20.32" y="-12.7" length="middle" direction="nc"/>
<pin name="NC1" x="-20.32" y="-15.24" length="middle" direction="nc"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="ESP32-WROOM-32E_(16MB)" prefix="U">
<description> &lt;a href="https://pricing.snapeda.com/parts/ESP32-WROOM-32E%20%2816MB%29/Espressif%20Systems/view-part?ref=eda"&gt;Check availability&lt;/a&gt;</description>
<gates>
<gate name="G$1" symbol="ESP32-WROOM-32E_(16MB)" x="0" y="0"/>
</gates>
<devices>
<device name="" package="XCVR_ESP32-WROOM-32E_(16MB)">
<connects>
<connect gate="G$1" pin="3V3" pad="2"/>
<connect gate="G$1" pin="EN" pad="3"/>
<connect gate="G$1" pin="GND" pad="1 15 38 39_1 39_2 39_3 39_4 39_5 39_6 39_7 39_8 39_9"/>
<connect gate="G$1" pin="IO0" pad="25"/>
<connect gate="G$1" pin="IO12" pad="14"/>
<connect gate="G$1" pin="IO13" pad="16"/>
<connect gate="G$1" pin="IO14" pad="13"/>
<connect gate="G$1" pin="IO15" pad="23"/>
<connect gate="G$1" pin="IO16" pad="27"/>
<connect gate="G$1" pin="IO17" pad="28"/>
<connect gate="G$1" pin="IO18" pad="30"/>
<connect gate="G$1" pin="IO19" pad="31"/>
<connect gate="G$1" pin="IO2" pad="24"/>
<connect gate="G$1" pin="IO21" pad="33"/>
<connect gate="G$1" pin="IO22" pad="36"/>
<connect gate="G$1" pin="IO23" pad="37"/>
<connect gate="G$1" pin="IO25" pad="10"/>
<connect gate="G$1" pin="IO26" pad="11"/>
<connect gate="G$1" pin="IO27" pad="12"/>
<connect gate="G$1" pin="IO32" pad="8"/>
<connect gate="G$1" pin="IO33" pad="9"/>
<connect gate="G$1" pin="IO34" pad="6"/>
<connect gate="G$1" pin="IO35" pad="7"/>
<connect gate="G$1" pin="IO4" pad="26"/>
<connect gate="G$1" pin="IO5" pad="29"/>
<connect gate="G$1" pin="NC" pad="17 18 19 20 21 22"/>
<connect gate="G$1" pin="NC1" pad="32"/>
<connect gate="G$1" pin="RXD0" pad="34"/>
<connect gate="G$1" pin="SENSOR_VN" pad="5"/>
<connect gate="G$1" pin="SENSOR_VP" pad="4"/>
<connect gate="G$1" pin="TXD0" pad="35"/>
</connects>
<technologies>
<technology name="">
<attribute name="AVAILABILITY" value="Not in stock"/>
<attribute name="DESCRIPTION" value=" Bluetooth, WiFi 802.11b/g/n, Bluetooth v4.2 +EDR, Class 1, 2 and 3 Transceiver Module 2.4GHz ~ 2.5GHz Integrated, Trace Surface Mount "/>
<attribute name="MF" value="Espressif Systems"/>
<attribute name="MP" value="ESP32-WROOM-32E (16MB)"/>
<attribute name="PACKAGE" value="SMD-44 Espressif Systems"/>
<attribute name="PRICE" value="None"/>
</technology>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0">
</class>
</classes>
<parts>
<part name="U1" library="ESP32-S3-WROOM-1-N16R2" deviceset="ESP32-S3-WROOM-1-N16R2" device=""/>
<part name="U2" library="ESP32-WROOM-32E__16MB_" deviceset="ESP32-WROOM-32E_(16MB)" device=""/>
</parts>
<sheets>
<sheet>
<plain>
</plain>
<instances>
<instance part="U1" gate="G$1" x="25.4" y="48.26" smashed="yes">
<attribute name="NAME" x="15.24" y="82.3722" size="1.778" layer="95"/>
<attribute name="VALUE" x="15.24" y="12.7" size="1.778" layer="96"/>
</instance>
<instance part="U2" gate="G$1" x="83.82" y="48.26" smashed="yes">
<attribute name="NAME" x="68.58" y="82.169" size="1.778" layer="95"/>
<attribute name="VALUE" x="68.58" y="15.24" size="1.778" layer="96"/>
</instance>
</instances>
<busses>
</busses>
<nets>
<net name="TXD0" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="TXD0"/>
<wire x1="63.5" y1="45.72" x2="55.88" y2="45.72" width="0.1524" layer="91"/>
<label x="55.88" y="45.72" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="TXD0"/>
<wire x1="10.16" y1="68.58" x2="2.54" y2="68.58" width="0.1524" layer="91"/>
<label x="2.54" y="68.58" size="1.016" layer="95"/>
</segment>
</net>
<net name="RXD0" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="RXD0"/>
<wire x1="63.5" y1="43.18" x2="55.88" y2="43.18" width="0.1524" layer="91"/>
<label x="55.88" y="43.18" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="RXD0"/>
<wire x1="10.16" y1="71.12" x2="2.54" y2="71.12" width="0.1524" layer="91"/>
<label x="2.54" y="71.12" size="1.016" layer="95"/>
</segment>
</net>
<net name="3V3" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="3V3"/>
<wire x1="40.64" y1="78.74" x2="48.26" y2="78.74" width="0.1524" layer="91"/>
<label x="45.72" y="78.74" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="3V3"/>
<wire x1="104.14" y1="78.74" x2="111.76" y2="78.74" width="0.1524" layer="91"/>
<label x="109.22" y="78.74" size="1.016" layer="95"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="GND"/>
<wire x1="40.64" y1="17.78" x2="48.26" y2="17.78" width="0.1524" layer="91"/>
<label x="45.72" y="17.78" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="GND"/>
<wire x1="104.14" y1="20.32" x2="111.76" y2="20.32" width="0.1524" layer="91"/>
<label x="109.22" y="20.32" size="1.016" layer="95"/>
</segment>
</net>
<net name="BOOT_BTN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO0"/>
<wire x1="10.16" y1="63.5" x2="2.54" y2="63.5" width="0.1524" layer="91"/>
<label x="2.54" y="63.5" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO0"/>
<wire x1="104.14" y1="73.66" x2="111.76" y2="73.66" width="0.1524" layer="91"/>
<label x="106.68" y="73.66" size="1.016" layer="95"/>
</segment>
</net>
<net name="PTT1_BTN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO3"/>
<wire x1="10.16" y1="55.88" x2="2.54" y2="55.88" width="0.1524" layer="91"/>
<label x="2.54" y="55.88" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO34"/>
<wire x1="63.5" y1="55.88" x2="55.88" y2="55.88" width="0.1524" layer="91"/>
<label x="55.88" y="55.88" size="1.016" layer="95"/>
</segment>
</net>
<net name="PMU_IRQ" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO4"/>
<wire x1="10.16" y1="53.34" x2="2.54" y2="53.34" width="0.1524" layer="91"/>
<label x="2.54" y="53.34" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO35"/>
<wire x1="63.5" y1="53.34" x2="55.88" y2="53.34" width="0.1524" layer="91"/>
<label x="55.88" y="53.34" size="1.016" layer="95"/>
</segment>
</net>
<net name="ENC_B" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO19"/>
<wire x1="104.14" y1="45.72" x2="111.76" y2="45.72" width="0.1524" layer="91"/>
<label x="106.68" y="45.72" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO46"/>
<wire x1="40.64" y1="27.94" x2="48.26" y2="27.94" width="0.1524" layer="91"/>
<label x="43.18" y="27.94" size="1.016" layer="95"/>
</segment>
</net>
<net name="ENC_SW" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO5"/>
<wire x1="104.14" y1="66.04" x2="111.76" y2="66.04" width="0.1524" layer="91"/>
<label x="106.68" y="66.04" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO21"/>
<wire x1="40.64" y1="53.34" x2="48.26" y2="53.34" width="0.1524" layer="91"/>
<label x="43.18" y="53.34" size="1.016" layer="95"/>
</segment>
</net>
<net name="ENC_A" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO18"/>
<wire x1="104.14" y1="48.26" x2="111.76" y2="48.26" width="0.1524" layer="91"/>
<label x="106.68" y="48.26" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO47"/>
<wire x1="40.64" y1="25.4" x2="48.26" y2="25.4" width="0.1524" layer="91"/>
<label x="43.18" y="25.4" size="1.016" layer="95"/>
</segment>
</net>
<net name="SD_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO13"/>
<wire x1="10.16" y1="30.48" x2="2.54" y2="30.48" width="0.1524" layer="91"/>
<label x="2.54" y="30.48" size="1.016" layer="95"/>
</segment>
</net>
<net name="SCL" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO22"/>
<wire x1="104.14" y1="40.64" x2="111.76" y2="40.64" width="0.1524" layer="91"/>
<label x="109.22" y="40.64" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO9"/>
<wire x1="10.16" y1="40.64" x2="2.54" y2="40.64" width="0.1524" layer="91"/>
<label x="2.54" y="40.64" size="1.016" layer="95"/>
</segment>
</net>
<net name="IO16-2" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO16"/>
<wire x1="104.14" y1="53.34" x2="111.76" y2="53.34" width="0.1524" layer="91"/>
<label x="106.68" y="53.34" size="1.016" layer="95"/>
</segment>
</net>
<net name="HL" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO25"/>
<wire x1="104.14" y1="35.56" x2="111.76" y2="35.56" width="0.1524" layer="91"/>
<label x="109.22" y="35.56" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO38"/>
<wire x1="40.64" y1="43.18" x2="48.26" y2="43.18" width="0.1524" layer="91"/>
<label x="45.72" y="43.18" size="1.016" layer="95"/>
</segment>
</net>
<net name="PD" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO40"/>
<wire x1="40.64" y1="38.1" x2="48.26" y2="38.1" width="0.1524" layer="91"/>
<label x="45.72" y="38.1" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO27"/>
<wire x1="104.14" y1="30.48" x2="111.76" y2="30.48" width="0.1524" layer="91"/>
<label x="109.22" y="30.48" size="1.016" layer="95"/>
</segment>
</net>
<net name="PTT" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO41"/>
<wire x1="40.64" y1="35.56" x2="48.26" y2="35.56" width="0.1524" layer="91"/>
<label x="45.72" y="35.56" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO32"/>
<wire x1="104.14" y1="27.94" x2="111.76" y2="27.94" width="0.1524" layer="91"/>
<label x="109.22" y="27.94" size="1.016" layer="95"/>
</segment>
</net>
<net name="RF_RXD" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO13"/>
<wire x1="104.14" y1="60.96" x2="111.76" y2="60.96" width="0.1524" layer="91"/>
<label x="106.68" y="60.96" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO39"/>
<wire x1="40.64" y1="40.64" x2="48.26" y2="40.64" width="0.1524" layer="91"/>
<label x="45.72" y="40.64" size="1.016" layer="95"/>
</segment>
</net>
<net name="RF_TXD" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO48"/>
<wire x1="40.64" y1="22.86" x2="48.26" y2="22.86" width="0.1524" layer="91"/>
<label x="43.18" y="22.86" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO14"/>
<wire x1="104.14" y1="58.42" x2="111.76" y2="58.42" width="0.1524" layer="91"/>
<label x="106.68" y="58.42" size="1.016" layer="95"/>
</segment>
</net>
<net name="AF_IN" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="SENSOR_VP"/>
<wire x1="63.5" y1="66.04" x2="55.88" y2="66.04" width="0.1524" layer="91"/>
<label x="55.88" y="66.04" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO1"/>
<wire x1="10.16" y1="60.96" x2="2.54" y2="60.96" width="0.1524" layer="91"/>
<label x="2.54" y="60.96" size="1.016" layer="95"/>
</segment>
</net>
<net name="N$42" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="SENSOR_VN"/>
<wire x1="63.5" y1="63.5" x2="55.88" y2="63.5" width="0.1524" layer="91"/>
</segment>
</net>
<net name="GPS_RXD" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO15"/>
<wire x1="104.14" y1="55.88" x2="111.76" y2="55.88" width="0.1524" layer="91"/>
<label x="106.68" y="55.88" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO6"/>
<wire x1="10.16" y1="48.26" x2="2.54" y2="48.26" width="0.1524" layer="91"/>
<label x="2.54" y="48.26" size="1.016" layer="95"/>
</segment>
</net>
<net name="GPS_TXD" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO17"/>
<wire x1="104.14" y1="50.8" x2="111.76" y2="50.8" width="0.1524" layer="91"/>
<label x="106.68" y="50.8" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO5"/>
<wire x1="10.16" y1="50.8" x2="2.54" y2="50.8" width="0.1524" layer="91"/>
<label x="2.54" y="50.8" size="1.016" layer="95"/>
</segment>
</net>
<net name="SDA" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO21"/>
<wire x1="104.14" y1="43.18" x2="111.76" y2="43.18" width="0.1524" layer="91"/>
<label x="109.22" y="43.18" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO8"/>
<wire x1="10.16" y1="43.18" x2="2.54" y2="43.18" width="0.1524" layer="91"/>
<label x="2.54" y="43.18" size="1.016" layer="95"/>
</segment>
</net>
<net name="AF_OUT" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO26"/>
<label x="106.68" y="33.02" size="1.016" layer="95"/>
<wire x1="111.76" y1="33.02" x2="104.14" y2="33.02" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO18"/>
<wire x1="40.64" y1="60.96" x2="48.26" y2="60.96" width="0.1524" layer="91"/>
<label x="43.18" y="60.96" size="1.016" layer="95"/>
</segment>
</net>
<net name="RXLED" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO2"/>
<wire x1="104.14" y1="71.12" x2="111.76" y2="71.12" width="0.1524" layer="91"/>
<label x="109.22" y="71.12" size="1.016" layer="95"/>
</segment>
</net>
<net name="RESET" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="EN"/>
<wire x1="63.5" y1="73.66" x2="55.88" y2="73.66" width="0.1524" layer="91"/>
<label x="55.88" y="73.66" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="EN"/>
<wire x1="10.16" y1="76.2" x2="2.54" y2="76.2" width="0.1524" layer="91"/>
<label x="2.54" y="76.2" size="1.016" layer="95"/>
</segment>
</net>
<net name="SQL" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="IO33"/>
<label x="109.22" y="25.4" size="1.016" layer="95"/>
<wire x1="104.14" y1="25.4" x2="111.76" y2="25.4" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="U1" gate="G$1" pin="IO14"/>
<wire x1="10.16" y1="27.94" x2="2.54" y2="27.94" width="0.1524" layer="91"/>
<label x="2.54" y="27.94" size="1.016" layer="95"/>
</segment>
</net>
<net name="MIC_CH_SEL" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO17"/>
<wire x1="40.64" y1="63.5" x2="48.26" y2="63.5" width="0.1524" layer="91"/>
<label x="43.18" y="63.5" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO23"/>
<wire x1="104.14" y1="38.1" x2="111.76" y2="38.1" width="0.1524" layer="91"/>
<label x="106.68" y="38.1" size="1.016" layer="95"/>
</segment>
</net>
<net name="SD_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO10"/>
<wire x1="10.16" y1="38.1" x2="2.54" y2="38.1" width="0.1524" layer="91"/>
<label x="2.54" y="38.1" size="1.016" layer="95"/>
</segment>
</net>
<net name="SD_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO11"/>
<wire x1="10.16" y1="35.56" x2="2.54" y2="35.56" width="0.1524" layer="91"/>
<label x="2.54" y="35.56" size="1.016" layer="95"/>
</segment>
</net>
<net name="SD_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO12"/>
<wire x1="10.16" y1="33.02" x2="2.54" y2="33.02" width="0.1524" layer="91"/>
<label x="2.54" y="33.02" size="1.016" layer="95"/>
</segment>
</net>
<net name="GPS_1PPS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO7"/>
<wire x1="10.16" y1="45.72" x2="2.54" y2="45.72" width="0.1524" layer="91"/>
<label x="2.54" y="45.72" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO12"/>
<wire x1="104.14" y1="63.5" x2="111.76" y2="63.5" width="0.1524" layer="91"/>
<label x="106.68" y="63.5" size="1.016" layer="95"/>
</segment>
</net>
<net name="PIXES" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO42"/>
<wire x1="40.64" y1="33.02" x2="48.26" y2="33.02" width="0.1524" layer="91"/>
<label x="45.72" y="33.02" size="1.016" layer="95"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="IO4"/>
<wire x1="104.14" y1="68.58" x2="111.76" y2="68.58" width="0.1524" layer="91"/>
<label x="106.68" y="68.58" size="1.016" layer="95"/>
</segment>
</net>
<net name="FREE_RX1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO15"/>
<wire x1="10.16" y1="25.4" x2="2.54" y2="25.4" width="0.1524" layer="91"/>
<label x="2.54" y="25.4" size="1.016" layer="95"/>
</segment>
</net>
<net name="FREE_TX1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO16"/>
<wire x1="10.16" y1="22.86" x2="2.54" y2="22.86" width="0.1524" layer="91"/>
<label x="2.54" y="22.86" size="1.016" layer="95"/>
</segment>
</net>
<net name="USB_N" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO19"/>
<wire x1="40.64" y1="58.42" x2="48.26" y2="58.42" width="0.1524" layer="91"/>
<label x="43.18" y="58.42" size="1.016" layer="95"/>
</segment>
</net>
<net name="USB_P" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="IO20"/>
<wire x1="40.64" y1="55.88" x2="48.26" y2="55.88" width="0.1524" layer="91"/>
<label x="43.18" y="55.88" size="1.016" layer="95"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>
