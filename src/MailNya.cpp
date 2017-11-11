#include "AttachmentNya.hpp"
#include "CommonMail.hpp"
#include "Rfc2822.hpp"

#include <QDir>
#include <QTextCodec>
#include <QUuid>

#include "MailNya.hpp"


namespace Nya
{
Mail::Mail(const QString& sender, const QString& subject, const QString& body)
	: sender(sender)
	, subject(subject)
	, text(body)
{}

Mail::Mail(const QByteArray& rfc2822)
{
	Rfc2822 parser(this);
	parser.Parse(rfc2822);
}

Mail::~Mail()
{}

/**
 * Get all recipients.
 */
QStringList Mail::GetRecipients(RecipientType type) const
{
	if (type == R_BCC) return rcptBcc;
	if (type == R_CC) return rcptCc;
	return rcptTo;
}

/**
 * Add recipient.
 */
void Mail::AddRecipient(const QString& recipient, RecipientType type)
{
	if (type == R_BCC) rcptBcc.append(recipient);
	else if (type == R_CC) rcptCc.append(recipient);
	else rcptTo.append(recipient);
}

/**
 * Remove recipient.
 */
void Mail::RemoveRecipient(const QString& recipient)
{
	rcptTo.removeAll(recipient);
	rcptCc.removeAll(recipient);
	rcptBcc.removeAll(recipient);
}

/**
 * Add extra header.
 */
void Mail::AddExtraHeader(const QByteArray& key, const QByteArray& value)
{
	extraHeaders[key.toLower()] = value;
}

/**
 * Remove extra header.
 */
void Mail::RemoveExtraHeader(const QByteArray& key)
{
	extraHeaders.remove(key.toLower());
}

/**
 * Add file attachment.
 */
void Mail::AddAttachment(const QString& filePath)
{
	auto* a = new Attachment(new QFile(filePath));
	AddAttachment(QFileInfo(filePath).fileName(), a);
}

/**
 * Add any attachment.
 */
void Mail::AddAttachment(const QString& fileName, Attachment* pA)
{
	s_p<Attachment> a(pA);
	QString newName = fileName;
	if (attachments.contains(newName))
	{
		QString baseName = QFileInfo(fileName).baseName();
		QString ext = QFileInfo(fileName).suffix();
		int i = 0;
		while (attachments.contains(newName))
		{
			++i;
			newName = QString("%1 (%2).%3").arg(baseName).arg(i).arg(ext);
		}
	}
	attachments[newName] = a;
}

/**
 * Remove attachment.
 */
void Mail::RemoveAttachment(const QString& filename)
{
	attachments.remove(filename);
}

/**
 * Convert to ASCII.
 */
Mail::operator QByteArray() const
{
	// headers
	QByteArray data;
	if (!sender.isEmpty() && !extraHeaders.contains("From"))
	{
		data += CreateEntity("From", sender);
	}
	if (!rcptTo.isEmpty())
	{
		data += CreateEntity("To", rcptTo.join(", "));
	}
	if (!rcptCc.isEmpty())
	{
		data += CreateEntity("Cc", rcptCc.join(", "));
	}
	if (!subject.isEmpty())
	{
		data += CreateEntity("Subject", subject);
	}

	// encoding
	QByteArray cte = extraHeaders["Content-Transfer-Encoding"].toLower();
	char enc = (cte == "base64") ? 'b' : (cte == "quoted-printable") ? 'q' : 0;
	if (!enc) enc = GuessEncoding(text);

	if (enc != 'a' && !extraHeaders.contains("MIME-Version") && !attachments.count())
	{
		data += "MIME-Version: 1.0\r\n";
	}

	if (attachments.count())
	{
		if (boundary.isEmpty())
			boundary = QUuid::createUuid().toString().toLatin1().replace("{", "").replace("}", "");
		if (!extraHeaders.contains("MIME-Version"))
			data += "MIME-Version: 1.0\r\n";
		if (!extraHeaders.contains("Content-Type"))
			data += "Content-Type: multipart/mixed; boundary=" + boundary + "\r\n";
	}
	else if (enc == 'b')
	{
		data += "Content-Transfer-Encoding: base64\r\n";
	}
	else if (enc == 'q')
	{
		data += "Content-Transfer-Encoding: quoted-printable\r\n";
	}

	for (auto i = extraHeaders.begin(); i != extraHeaders.end(); ++i)
	{
		QByteArray key = i.key();
		if ((key.toLower() == "content-type" || key.toLower() == "content-transfer-encoding") && attachments.count())
		{
			// Since we're in multipart mode, we'll be outputting this later
			continue;
		}
		data += CreateEntity(key, i.value());
	}
	data += "\r\n";

	if (attachments.count())
	{
		// we're going to have attachments, so output the lead-in for the message body
		data += "This is a message with multiple parts in MIME format.\r\n";
		data += "--" + boundary + "\r\nContent-Type: ";
		if (extraHeaders.contains("Content-Type"))
		{
			data += extraHeaders["Content-Type"] + "\r\n";
		}
		else
		{
			data += "text/plain; charset=UTF-8\r\n";
		}

		if (extraHeaders.contains("Content-Transfer-Encoding"))
		{
			data += "Content-Transfer-Encoding: " + extraHeaders["Content-Transfer-Encoding"] + "\r\n";
		}
		else if (enc == 'b')
		{
			data += "Content-Transfer-Encoding: base64\r\n";
		}
		else if (enc == 'q')
		{
			data += "Content-Transfer-Encoding: quoted-printable\r\n";
		}
		data += "\r\n";
	}

	if (enc == 'a')
	{
		QByteArray ba = text.toLatin1();
		int len = ba.size();
		QByteArray line;
		QByteArray word;
		QByteArray spaces;
		QByteArray startSpaces;
		for (int i = 0; i <= len; ++i)
		{
			char ignoredChar = 0;
			if (i != len)
			{
				ignoredChar = (ba[i] == '\n') ? '\r' : (ba[i] == '\r') ? '\n' : 0;
			}

			if (!(ignoredChar || (i == len) || (ba[i] == ' ') || (ba[i] == '\t')))
			{
				// the char is part of word
				if (word.isEmpty())
				{
					// start of new word / end of spaces
					if (line.isEmpty()) startSpaces = spaces;
				}
				word += ba[i];
				continue;
			}

			// space char, so end of word or continuous spaces
			if (!word.isEmpty())
			{
				if (line.length() + spaces.length() + word.length() > wordWrap)
				{
					// have to wrap word to next line
					if(line[0] == '.')
						data += ".";
					data += line + "\r\n";
					if (isKeepIndentation) line = startSpaces + word;
					else line = word;
				}
				else // no wrap required
				{
					line += spaces + word;
				}
				word.clear();
				spaces.clear();
			}

			if (ignoredChar || i == len)
			{
				// trailing `spaces` are ignored here
				if(line[0] == '.') data += ".";
				data += line + "\r\n";
				line.clear();
				startSpaces.clear();
				spaces.clear();
			}
			else spaces += ba[i];
		}
	}
	else if (enc == 'b')
	{
		QByteArray ba = text.toUtf8().toBase64();
		for (int i = 0; i < ba.size(); i += 78)
		{
			data += ba.mid(i, 78) + "\r\n";
		}
	}
	else if (enc == 'q')
	{
		QByteArray ba = text.toUtf8();
		QByteArray line;
		for (int i = 0; i < ba.size(); ++i)
		{
			if(ba[i] == '\n' || ba[i] == '\r')
			{
				if(line[0] == '.') data += ".";

				data += line + "\r\n";
				line = "";
				if ((ba[i + 1] == '\n' || ba[i + 1] == '\r') && ba[i] != ba[i + 1])
				{
					// If we're looking at a CRLF pair, skip the second half
					++i;
				}
			}
			else if (line.length() > 74)
			{
				data += line + "=\r\n";
				line = "";
			}

			if (IsSpecialChar(ba[i]))
			{
				line += "=" + ba.mid(i, 1).toHex().toUpper();
			}
			else line += ba[i];
		}
		if (!line.isEmpty())
		{
			if(line[0] == '.') data += ".";
			data += line + "\r\n";
		}
	}

	if (attachments.count())
	{
		for (auto i = attachments.begin(); i != attachments.end(); ++i)
		{
			QString filename = i.key();
			data += "--" + boundary + "\r\n";
			data += CreateEntity("Content-Disposition", QDir(filename).dirName(), "attachment; filename=");
			data += i.value()->MimeData();
		}
		data += "--" + boundary + "--\r\n";
	}
	return data;
}

}

