QT = core network
TEMPLATE = lib

# Remove this lines if you don't want them (git update-index --assume-unchanged nya_smtp.pro).
include(../nya/pri/vars.pri)
include(../nya/pri/common.pri)


HEADERS += \
	src/SmtpNya.hpp \
	src/AttachmentNya.hpp \
	src/MailNya.hpp \
	src/Rfc2822.hpp \
	src/CommonMail.hpp

SOURCES += \
	src/SmtpNya.cpp \
	src/AttachmentNya.cpp \
	src/MailNya.cpp \
	src/Rfc2822.cpp \
	src/CommonMail.cpp
