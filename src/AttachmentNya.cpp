#include "CommonMail.hpp"
#include "MailNya.hpp"

#include <QBuffer>
#include <QFile>

#include "AttachmentNya.hpp"


namespace Nya
{
/**
 * Construct from file.
 */
Attachment::Attachment(const QString& filePath, const QByteArray& contentType)
	: contentType(contentType)
	, content(new QFile(filePath))
{}

/**
 * Construct taking ownership of device.
 */
Attachment::Attachment(QIODevice* device, const QByteArray& contentType)
	: contentType(contentType)
	, content(device)
{}

/**
 * Construct with QBuffer.
 */
Attachment::Attachment(const QByteArray* ba, const QByteArray& contentType)
	: contentType(contentType)
	, content(new QBuffer)
{
	std::static_pointer_cast<QBuffer>(content)->setData(*ba);
}

Attachment::~Attachment()
{}

/**
 * Mime data.
 */
QByteArray Attachment::MimeData() const
{
	QByteArray data = "Content-Type: " + contentType + "\r\nContent-Transfer-Encoding: base64\r\n";
	for (auto i = extraHeaders.begin(); i != extraHeaders.end(); ++i )
	{
		data += CreateEntity(i.key(), i.value());
	}
	data += "\r\n";

	if (content)
	{
		content->open(QIODevice::ReadOnly);
		QByteArray contentData = content->readAll();
		for (int pos = 0; pos < contentData.length(); pos += 57)
		{
			data += contentData.mid(pos, 57).toBase64() + "\r\n";
		}
		content->close();
	}
	return data;
}
}
