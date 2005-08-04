<?xml version="1.0" encoding="iso-8859-15"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- $Id$ -->

  <xsl:output method="xml" version="1.0" encoding="iso-8859-15" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd" indent="yes"/>

  <xsl:template match="/">
    <html xmlns="http://www.w3.org/1999/xhtml">
      <head>
        <title>DBUS Introspection data</title>
        <style type="text/css">
          body { color: black; background-color: white } 
          h1 { font-family: sans-serif }
          ul { list-style-type: none; margin-bottom: 10px }
          li { font-family: sans-serif }
          .keyword { font-style: italic }
          .type { font-weight: bold }
          .symbol { font-family: monospace }
          .interface { background: #efefef; padding: 10px; margin: 10px }
        </style>
      </head>
      <body>
        <xsl:for-each select="node/interface">
          <div class="interface">
            <h1>
              <span class="keyword">interface</span><xsl:text> </xsl:text>
              <span class="symbol"><xsl:value-of select="@name"/></span>
            </h1>   
            
            <ul>

            <xsl:apply-templates select="annotation"/> 

            <xsl:for-each select="method|signal|property">
              <li>
                <span class="keyword"><xsl:value-of select="name()"/></span>
                <xsl:text> </xsl:text>
                <span class="symbol"><xsl:value-of select="@name"/></span>
                
                <ul>
                  <xsl:apply-templates select="annotation"/> 
                  <xsl:for-each select="arg">
                    <li>
                      <span class="keyword">
                        <xsl:choose>
                          <xsl:when test="@direction != &quot;&quot;">
                            <xsl:value-of select="@direction"/> 
                          </xsl:when>
                          <xsl:when test="name(..) = &quot;signal&quot;">
                            out
                          </xsl:when>
                          <xsl:otherwise>
                            in
                          </xsl:otherwise>
                        </xsl:choose>
                      </span>

                      <xsl:text> </xsl:text>
                      
                      <span class="type"><xsl:value-of select="@type"/></span><xsl:text> </xsl:text>
                      <span class="symbol"><xsl:value-of select="@name"/></span><xsl:text> </xsl:text>
                    </li>
                  </xsl:for-each>
                </ul>

              </li>
            </xsl:for-each>

            </ul>
          </div>
        </xsl:for-each>
      </body>
    </html>
  </xsl:template>


  <xsl:template match="annotation"> 
    <li xmlns="http://www.w3.org/1999/xhtml">
      <span class="keyword">annotation</span>
      <code><xsl:value-of select="@name"/></code><xsl:text> = </xsl:text>
      <code><xsl:value-of select="@value"/></code>
    </li>
  </xsl:template>

</xsl:stylesheet>
