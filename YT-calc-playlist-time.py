import tkinter as tk
from tkinter import messagebox, simpledialog
import re
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from urllib.parse import parse_qs, urlparse

API_KEY = None

def get_playlist_id(url):
    """Extract playlist ID from various forms of YouTube URLs."""
    query = parse_qs(urlparse(url).query)
    return query.get("list", [None])[0]

def parse_duration(duration):
    """Convert YouTube duration format to seconds."""
    match = re.match(r'PT(\d+H)?(\d+M)?(\d+S)?', duration)
    if not match:
        return 0
    hours = int(match.group(1)[:-1]) if match.group(1) else 0
    minutes = int(match.group(2)[:-1]) if match.group(2) else 0
    seconds = int(match.group(3)[:-1]) if match.group(3) else 0
    return hours * 3600 + minutes * 60 + seconds

def get_playlist_duration(playlist_id):
    """Calculate total duration of all videos in a playlist."""
    youtube = build('youtube', 'v3', developerKey=API_KEY)
    total_seconds = 0
    next_page_token = None

    while True:
        try:
            # Get playlist items
            pl_request = youtube.playlistItems().list(
                part='contentDetails',
                playlistId=playlist_id,
                maxResults=50,
                pageToken=next_page_token
            )
            pl_response = pl_request.execute()

            # Get video IDs
            video_ids = [item['contentDetails']['videoId'] for item in pl_response['items']]

            # Get video details
            vid_request = youtube.videos().list(
                part="contentDetails",
                id=','.join(video_ids)
            )
            vid_response = vid_request.execute()

            # Sum up the durations
            for item in vid_response['items']:
                duration = item['contentDetails']['duration']
                total_seconds += parse_duration(duration)

            next_page_token = pl_response.get('nextPageToken')
            if not next_page_token:
                break

        except HttpError as e:
            print(f'An HTTP error {e.resp.status} occurred: {e.content}')
            return None

    return total_seconds

def format_duration(seconds):
    """Format duration in seconds to HH:MM:SS."""
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"

def calculate_duration():
    """Handle button click event."""
    global API_KEY
    url = url_entry.get()
    playlist_id = get_playlist_id(url)
    
    if not playlist_id:
        messagebox.showerror("Error", "Invalid YouTube playlist URL")
        return

    if not API_KEY:
        API_KEY = simpledialog.askstring("Input", "Please enter your YouTube API Key:", show='*')
        if not API_KEY:
            messagebox.showerror("Error", "API Key is required")
            return

    result_label.config(text="Calculating...")
    root.update()

    total_seconds = get_playlist_duration(playlist_id)
    
    if total_seconds is not None:
        formatted_duration = format_duration(total_seconds)
        result_label.config(text=f"Total duration: {formatted_duration}")
    else:
        result_label.config(text="An error occurred. Please check your API key and try again.")
        API_KEY = None  # Reset API key if there was an error

def set_api_key():
    """Handle setting API key."""
    global API_KEY
    API_KEY = simpledialog.askstring("Input", "Please enter your YouTube API Key:", show='*')
    if API_KEY:
        messagebox.showinfo("Success", "API Key has been set")
    else:
        messagebox.showwarning("Warning", "API Key was not set")

# Create main window
root = tk.Tk()
root.title("YouTube Playlist Duration Calculator")
root.geometry("400x200")

# Create and place widgets
tk.Label(root, text="Enter YouTube Playlist URL:").pack(pady=10)
url_entry = tk.Entry(root, width=50)
url_entry.pack()

calculate_button = tk.Button(root, text="Calculate Duration", command=calculate_duration)
calculate_button.pack(pady=10)

set_api_key_button = tk.Button(root, text="Set API Key", command=set_api_key)
set_api_key_button.pack(pady=5)

result_label = tk.Label(root, text="")
result_label.pack()

# Start the GUI event loop
root.mainloop()
