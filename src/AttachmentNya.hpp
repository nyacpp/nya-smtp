#ifndef ATTACHMENTNYA_H
#define ATTACHMENTNYA_H

#include "CommonMail.hpp"
#include <QIODevice>
#include <QHash>
#include <QByteArray>
#include <QStringList>


namespace Nya
{
class Attachment
{
	QByteArray contentType;
	mutable s_p<QIODevice> content;
	QHash<QByteArray, QByteArray> extraHeaders;

public:
	Attachment() : contentType("application/octet-stream") {}
	Attachment(const QString& filePath, const QByteArray& contentType = "application/octet-stream");
	Attachment(QIODevice* device, const QByteArray& contentType = "application/octet-stream");
	Attachment(const QByteArray* ba, const QByteArray& contentType = "application/octet-stream");
	virtual ~Attachment();

	QByteArray GetContentType() const { return contentType; }
	QHash<QByteArray, QByteArray>& GetExtraHeaders() { return extraHeaders; }

	void SetContentType(const QByteArray& contentType) { this->contentType = contentType;}

	QByteArray MimeData() const;
};

}
#endif // ATTACHMENTNYA_H
