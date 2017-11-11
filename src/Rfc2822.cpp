#include "AttachmentNya.hpp"
#include "CommonMail.hpp"
#include "MailNya.hpp"

#include <QRegExp>
#include <QTextCodec>
#include <QTextStream>

#include "Rfc2822.hpp"


namespace Nya
{
/**
 * Parse all.
 */
void Rfc2822::Parse(const QByteArray& buffer)
{
	ParseEntity(buffer, mail->extraHeaders, mail->text);
	ParseBody();
}

/**
 * Entity.
 */
void Rfc2822::ParseEntity(const QByteArray& buffer, QHash<QByteArray, QByteArray>& headers, QString& body)
{
	int pos = 0;
	int crlfPos = 0;
	QByteArray line;
	currentHeaderKey = QByteArray();
	currentHeaderValue.clear();

	bool isHeaders = true;
	while (true)
	{
		crlfPos = buffer.indexOf("\r\n", pos);
		if (crlfPos == -1)
		{
			break;
		}
		if (isHeaders && crlfPos == pos)  // double CRLF reached: end of headers section
		{
			isHeaders = false;
			ParseHeader("", headers); // to store the header currently being parsed (last one)
			pos += 2;
			continue;
		}
		line = buffer.mid(pos, crlfPos - pos);
		pos = crlfPos + 2;

		if( isHeaders ) ParseHeader(line, headers);
		else body.append(line + "\r\n");
	}
}

/**
 * Header.
 */
void Rfc2822::ParseHeader(const QByteArray& line, QHash<QByteArray, QByteArray>& headers)
{
	QRegExp spRe("^[ \\t]");
	QRegExp hdrRe("^([!-9;-~]+):[ \\t](.*)$");
	if (spRe.indexIn(line) == 0) // continuation line
	{
		currentHeaderValue.append(line);
	}
	else
	{
		// starting a new header field. Store the current one before
		if (!currentHeaderKey.isEmpty())
		{
			QString value = UnfoldValue(currentHeaderValue);
			headers[currentHeaderKey.toLower()] = value.toUtf8();
			currentHeaderKey = QByteArray();
			currentHeaderValue.clear();
		}
		if (hdrRe.exactMatch(line))
		{
			currentHeaderKey = hdrRe.cap(1).toUtf8();
			currentHeaderValue.append(hdrRe.cap(2));
		} // else: empty or malformed header line. Ignore.
	}
}

/**
 * Extract the attachments from a multipart body.
 * Proceed only one level deep.
 */
void Rfc2822::ParseBody()
{
	QString& text = mail->text;
	if (!mail->extraHeaders.contains("content-type")) return;
	QString contentType = mail->extraHeaders["content-type"];
	if (!contentType.indexOf("multipart",0,Qt::CaseInsensitive) == 0) return;

	// extract the boundary delimiter
	QRegExp boundaryRe("boundary=\"?([^\"]*)\"?(?=;|$)");
	if (boundaryRe.indexIn(contentType) == -1)
	{
		qDebug("Boundary regexp didn't match for %s", contentType.toLatin1().data());
		return;
	}

	// find boundary delimiters in the body
	QString boundary = boundaryRe.cap(1);
	QRegExp bndRe(QString("(^|\\r?\\n)--%1(--)?[ \\t]*\\r?\\n").arg(QRegExp::escape(boundary)));
	if (!bndRe.isValid())
	{
		qDebug("regexp %s not valid ! %s", bndRe.pattern().toLatin1().data(), bndRe.errorString().toLatin1().data());
	}

	// keep track of the position of two consecutive boundary delimiters:
	// begin* is the position of the delimiter first character,
	// end* is the position of the first character of the part following it.
	int beginFirst = 0;
	int endFirst = 0;
	int beginSecond = 0;
	int endSecond = 0;
	while(bndRe.indexIn(text, endSecond) != -1)
	{
		beginSecond = bndRe.pos() + bndRe.cap(1).length(); // add length of preceding line break, if any
		endSecond = bndRe.pos() + bndRe.matchedLength();

		if (endFirst != 0)
		{
			// handle part here:
			QByteArray part = text.mid(endFirst, beginSecond - endFirst).toLocal8Bit();
			QHash<QByteArray, QByteArray> partHeaders;
			QString partBody;
			ParseEntity(part, partHeaders, partBody);

			if (partHeaders.contains("content-disposition") && partHeaders["content-disposition"].indexOf("attachment;") == 0)
			{
				QString filename;
				if (auto a = ParseAttachment(partHeaders, partBody, filename))
				{
					mail->attachments.insert(filename, a);
				}
				// strip part from body
				text.replace(beginFirst, beginSecond - beginFirst, "");
				beginSecond = beginFirst;
				endSecond = endFirst;
			}
		}
		beginFirst = beginSecond;
		endFirst = endSecond;
	}
}

QString Rfc2822::UnfoldValue(QStringList& folded)
{
	QString unfolded;
	QRegExp encRe("=\\?([^? \\t]+)\\?([qQbB])\\?([^? \\t]+)\\?="); // search for an encoded word
	QStringList::iterator i;
	for (i = folded.begin(); i != folded.end(); ++i)
	{
		int offset = 0;
		while (encRe.indexIn(*i, offset) != -1)
		{
			QString decoded = Decode(encRe.cap(1), encRe.cap(2).toLower(), encRe.cap(3));
			i->replace(encRe.pos(), encRe.matchedLength(), decoded);  // replace encoded word with decoded one
			offset = encRe.pos() + decoded.length(); // set offset after the inserted decoded word
		}
	}
	unfolded = folded.join("");
	return unfolded;
}

/**
 * Decode.
 */
QString Rfc2822::Decode(const QString& charset, const QString& encoding, const QString& encoded)
{
	QString rv;
	QByteArray buf;
	if (encoding == "q")
	{
		QByteArray src = encoded.toLatin1();
		int len = src.length();
		for (int i = 0; i < len; i++)
		{
			if (src[i] == '_')
			{
				buf += 0x20;
			}
			else if (src[i] == '=')
			{
				if (i+2 < len)
				{
					buf += QByteArray::fromHex(src.mid(i+1,2));
					i += 2;
				}
			}
			else
			{
				buf += src[i];
			}
		}
	}
	else if (encoding == "b")
	{
		buf = QByteArray::fromBase64(encoded.toLatin1());
	}
	QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1());
	if (codec)
	{
		rv = codec->toUnicode(buf);
	}
	return rv;
}

/**
 * Attachment.
 */
s_p<Attachment> Rfc2822::ParseAttachment(const QHash<QByteArray, QByteArray>& headers, const QString& body, QString& filename)
{
	static int count = 1;
	QByteArray content;
	QRegExp filenameRe(";\\s+filename=\"?([^\"]*)\"?(?=;|$)");
	if (filenameRe.indexIn(headers["content-disposition"]) != -1)
	{
		filename = filenameRe.cap(1);
	}
	else
	{
		filename = QString("attachment%1").arg(count);
	}

	QByteArray contentType;
	if (headers.contains("content-type"))
	{
		contentType = headers["content-type"];
	}
	else
	{
		contentType = "application/octet-stream";
	}

	QString cte;
	if (headers.contains("content-transfer-encoding"))
	{
		cte = headers["content-transfer-encoding"].toLower();
	}
	if ( cte == "base64")
	{
		content = QByteArray::fromBase64(body.toLatin1());
	}
	else if (cte == "quoted-printable")
	{
		QByteArray src = body.toLatin1();
		int len = src.length();
		QTextStream dest(&content);
		for (int i = 0; i < len; i++)
		{
			if (src.mid(i,2) == "\r\n")
			{
				dest << endl;
				i++;
			}
			else if (src[i] == '=')
			{
				if (i+2 < len)
				{
					if (src.mid(i+1,2) == "\r\n") // soft line break; skip
					{
						i +=2;
					}
					else
					{
						dest << QByteArray::fromHex(src.mid(i+1,2));
						i += 2;
					}
				}
			}
			else
			{
				dest << src[i];
			}
		}
	}
	else // assume 7bit or 8bit
	{
		content = body.toLocal8Bit();
		if (IsText(contentType))
		{
			content.replace("\r\n","\n");
		}
	}
	s_p<Attachment> a(new Attachment(content, contentType));
	a->GetExtraHeaders() = headers;
	return a;
}
}
