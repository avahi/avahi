AM_CFLAGS=-I$(top_srcdir)

avahiincludedir=$(includedir)/avahi-qt

lib_LTLIBRARIES = 
BUILT_SOURCES = 

if HAVE_QT3

avahiinclude_HEADERS = \
	qt-watch.h 

lib_LTLIBRARIES += \
	libavahi-qt3.la

BUILT_SOURCES += qt-watch.moc3

libavahi_qt3_la_SOURCES = \
	qt-watch.cpp 

qt-watch.moc3: 
	$(MOC_QT3) qt-watch.cpp > $@


libavahi_qt3_la_CPPFLAGS = $(AM_CFLAGS) $(QT3_CFLAGS)
libavahi_qt3_la_LIBADD = $(AM_LDADD) ../avahi-common/libavahi-common.la $(QT3_LIBS)
libavahi_qt3_la_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -version-info 0:1:0

endif


if HAVE_QT4

avahiinclude_HEADERS = \
	qt-watch.h 

lib_LTLIBRARIES += \
	libavahi-qt4.la

BUILT_SOURCES += qt-watch.moc4

libavahi_qt4_la_SOURCES = \
	qt-watch.cpp 

qt-watch.moc4: 
	$(MOC_QT4) qt-watch.cpp > $@


libavahi_qt4_la_CPPFLAGS = $(AM_CFLAGS) $(QT4_CFLAGS) -DQT4
libavahi_qt4_la_LIBADD = $(AM_LDADD) ../avahi-common/libavahi-common.la $(QT4_LIBS)
libavahi_qt4_la_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -version-info 0:1:0

endif