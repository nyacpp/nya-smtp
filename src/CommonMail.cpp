#include <QTextCodec>
#include <QRegExp>

#include "CommonMail.hpp"


namespace Nya
{
/**
 * Guess encoding based on first 100 chars.
 */
char GuessEncoding(const QString& s)
{
	QTextCodec* latin1 = QTextCodec::codecForName("latin1");
	if (!s.contains("=?") && latin1->canEncode(s)) return 'a';

	int nonAscii = 0;
	for (int i = 0; i < s.size() && i < 100; ++i)
	{
		if (IsSpecialChar(s[i].toLatin1())) ++nonAscii;
	}
	return (nonAscii > 20) ? 'b' : 'q';
}

/**
 * Create mime entity.
 */
QByteArray CreateEntity(const QByteArray& key, const QString& value, const QByteArray& prefix)
{
	QByteArray data = "";
	QByteArray line = key + ": ";
	if (!prefix.isEmpty()) line += prefix;

	switch (GuessEncoding(value))
	{
	case 'a':
	{
		bool firstWord = true;
		for (const QByteArray& word : value.toLatin1().split(' '))
		{
			if (line.size() > 78) { data += line + "\r\n"; line.clear(); }

			if (firstWord) { firstWord = false; line += word; }
			else { line += " " + word; }
		}
		break;
	}
	case 'b': // base64
	{
		// > 20% non-ASCII characters
		QByteArray base64 = value.toUtf8().toBase64();

		line += "=?utf-8?b?";
		for (int i = 0; i < base64.size(); i += 4)
		{
			if (line.length() > 72)
			{
				data += line + "?=\r\n";
				line = " =?utf-8?b?";
			}
			line += base64.mid(i, 4);
		}
		line += "?=";
		break;
	}
	case 'q': // QP
	{
		QByteArray utf8 = value.toUtf8();

		line += "=?utf-8?q?";
		for (int i = 0; i < utf8.size(); ++i)
		{
			if (line.length() > 73)
			{
				data += line + "?=\r\n";
				line = " =?utf-8?q?";
			}

			if (IsSpecialChar(utf8[i]) || utf8[i] == ' ')
			{
				line += "=" + utf8.mid(i, 1).toHex().toUpper();
			}
			else line += utf8[i];
		}
		line += "?=";
		break;
	}
	default:;
	}
	return data + line + "\r\n";
}

/**
 * True if text, false if unsure.
 */
bool IsText(const QByteArray& contentType)
{
	// extract media type/sub-type part (e.g. text/plain, image/png...)
	QRegExp mtRe("^([^;/]*)/([^;/]*)(?=;|$)");
	if (mtRe.indexIn(contentType) == -1) return false;

	QString type = mtRe.cap(1).toLower();
	QString subtype = mtRe.cap(2).toLower();
	if (type == "text") return true;
	if (type == "application" &&
		(subtype == "x-csh" ||
		 subtype == "x-desktop" ||
		 subtype == "x-m4" ||
		 subtype == "x-perl" ||
		 subtype == "x-php" ||
		 subtype == "x-ruby" ||
		 subtype == "x-troff-man" ||
		 subtype == "xsd" ||
		 subtype == "xml-dtd" ||
		 subtype == "xml-external-parsed-entity" ||
		 subtype == "xslt+xml" ||
		 subtype == "xhtml+xml" ||
		 subtype == "pgp-keys" ||
		 subtype == "pgp-signature" ||
		 subtype == "javascript" ||
		 subtype == "ecmascript" ||
		 subtype == "docbook+xml" ||
		 subtype == "xml" ||
		 subtype == "html" ||
		 subtype == "x-shellscript")) return true;
	if (type == "image" && subtype == "svg+xml") return true;
	return false;
}
}
