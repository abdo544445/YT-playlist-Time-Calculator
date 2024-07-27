#include <QtWidgets>
#include <QtNetwork>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class PlaylistDurationCalculator : public QWidget {
    Q_OBJECT

public:
    PlaylistDurationCalculator(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Enter YouTube Playlist URL:", this);
        layout->addWidget(label);

        urlInput = new QLineEdit(this);
        layout->addWidget(urlInput);

        calculateButton = new QPushButton("Calculate Duration", this);
        layout->addWidget(calculateButton);

        resultLabel = new QLabel(this);
        layout->addWidget(resultLabel);

        connect(calculateButton, &QPushButton::clicked, this, &PlaylistDurationCalculator::calculateDuration);

        setLayout(layout);
        setWindowTitle("YouTube Playlist Duration Calculator");
    }

private slots:
    void calculateDuration() {
        QString url = urlInput->text();
        if (!url.contains("list=")) {
            resultLabel->setText("Invalid playlist URL");
            return;
        }

        QString playlistId = url.split("list=").last().split("&").first();
        QString apiKey = "YOUR_API_KEY_HERE"; // Replace with your actual YouTube Data API key

        QString apiUrl = QString("https://www.googleapis.com/youtube/v3/playlistItems?part=contentDetails&maxResults=50&playlistId=%1&key=%2").arg(playlistId, apiKey);

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished, this, &PlaylistDurationCalculator::onPlaylistItemsReceived);
        manager->get(QNetworkRequest(QUrl(apiUrl)));
    }

    void onPlaylistItemsReceived(QNetworkReply *reply) {
        if (reply->error()) {
            resultLabel->setText("Error: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
        QJsonArray items = jsonResponse.object()["items"].toArray();

        QStringList videoIds;
        for (const QJsonValue &item : items) {
            QString videoId = item.toObject()["contentDetails"].toObject()["videoId"].toString();
            videoIds.append(videoId);
        }

        QString apiKey = "YOUR_API_KEY_HERE"; // Replace with your actual YouTube Data API key
        QString apiUrl = QString("https://www.googleapis.com/youtube/v3/videos?part=contentDetails&id=%1&key=%2").arg(videoIds.join(","), apiKey);

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished, this, &PlaylistDurationCalculator::onVideoDurationsReceived);
        manager->get(QNetworkRequest(QUrl(apiUrl)));

        reply->deleteLater();
    }

    void onVideoDurationsReceived(QNetworkReply *reply) {
        if (reply->error()) {
            resultLabel->setText("Error: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
        QJsonArray items = jsonResponse.object()["items"].toArray();

        int totalSeconds = 0;
        for (const QJsonValue &item : items) {
            QString duration = item.toObject()["contentDetails"].toObject()["duration"].toString();
            totalSeconds += parseDuration(duration);
        }

        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;

        QString result = QString("Total duration: %1:%2:%3")
                             .arg(hours, 2, 10, QChar('0'))
                             .arg(minutes, 2, 10, QChar('0'))
                             .arg(seconds, 2, 10, QChar('0'));
        resultLabel->setText(result);

        reply->deleteLater();
    }

    int parseDuration(const QString &duration) {
        QRegularExpression re("PT(?:([0-9]+)H)?(?:([0-9]+)M)?(?:([0-9]+)S)?");
        QRegularExpressionMatch match = re.match(duration);

        int hours = match.captured(1).toInt();
        int minutes = match.captured(2).toInt();
        int seconds = match.captured(3).toInt();

        return hours * 3600 + minutes * 60 + seconds;
    }

private:
    QLineEdit *urlInput;
    QPushButton *calculateButton;
    QLabel *resultLabel;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    PlaylistDurationCalculator calculator;
    calculator.show();
    return app.exec();
}

#include "main.moc"
