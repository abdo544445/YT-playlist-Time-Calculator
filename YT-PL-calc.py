import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import re
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from urllib.parse import parse_qs, urlparse

API_KEY = None
videos = []

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

def get_playlist_videos(playlist_id):
    """Get all videos in a playlist with their durations."""
    youtube = build('youtube', 'v3', developerKey=API_KEY)
    videos = []
    next_page_token = None

    while True:
        try:
            # Get playlist items
            pl_request = youtube.playlistItems().list(
                part='contentDetails,snippet',
                playlistId=playlist_id,
                maxResults=50,
                pageToken=next_page_token
            )
            pl_response = pl_request.execute()

            # Get video details
            video_ids = [item['contentDetails']['videoId'] for item in pl_response['items']]
            vid_request = youtube.videos().list(
                part="contentDetails",
                id=','.join(video_ids)
            )
            vid_response = vid_request.execute()

            # Combine playlist and video data
            for pl_item, vid_item in zip(pl_response['items'], vid_response['items']):
                title = pl_item['snippet']['title']
                duration = parse_duration(vid_item['contentDetails']['duration'])
                videos.append({'title': title, 'duration': duration, 'watched': tk.BooleanVar(value=False)})

            next_page_token = pl_response.get('nextPageToken')
            if not next_page_token:
                break

        except HttpError as e:
            print(f'An HTTP error {e.resp.status} occurred: {e.content}')
            return None

    return videos

def format_duration(seconds):
    """Format duration in seconds to HH:MM:SS."""
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"

def calculate_duration():
    """Handle button click event to fetch playlist data."""
    global API_KEY, videos
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

    progress_bar.start()
    result_label.config(text="Fetching playlist data...")
    root.update()

    videos = get_playlist_videos(playlist_id)
    
    if videos:
        populate_video_list()
        update_remaining_time()
        progress_bar.stop()
        progress_bar.grid_remove()
    else:
        result_label.config(text="An error occurred. Please check your API key and try again.")
        API_KEY = None  # Reset API key if there was an error
        progress_bar.stop()
        progress_bar.grid_remove()

def set_api_key():
    """Handle setting API key."""
    global API_KEY
    API_KEY = simpledialog.askstring("Input", "Please enter your YouTube API Key:", show='*')
    if API_KEY:
        messagebox.showinfo("Success", "API Key has been set")
    else:
        messagebox.showwarning("Warning", "API Key was not set")

def populate_video_list():
    """Populate the treeview with video titles and checkboxes."""
    for item in tree.get_children():
        tree.delete(item)
    for i, video in enumerate(videos):
        tree.insert('', 'end', values=(video['title'], format_duration(video['duration'])), tags=(i,))

def toggle_watched(event):
    """Toggle watched status of selected video."""
    item = tree.focus()
    if item:
        tags = tree.item(item, "tags")
        if tags:
            index = int(tags[0])
            videos[index]['watched'].set(not videos[index]['watched'].get())
            update_remaining_time()

def update_remaining_time():
    """Update the remaining time based on unwatched videos."""
    total_seconds = sum(video['duration'] for video in videos if not video['watched'].get())
    remaining_time = format_duration(total_seconds)
    result_label.config(text=f"Remaining time: {remaining_time}")

# Create main window
root = tk.Tk()
root.title("YouTube Playlist Duration Calculator")
root.geometry("800x600")

style = ttk.Style()
style.theme_use('clam')  # You can try different themes like 'alt', 'default', 'classic'

# Create and place widgets
main_frame = ttk.Frame(root, padding="10")
main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
root.columnconfigure(0, weight=1)
root.rowconfigure(0, weight=1)

ttk.Label(main_frame, text="Enter YouTube Playlist URL:").grid(column=0, row=0, sticky=tk.W, pady=5)
url_entry = ttk.Entry(main_frame, width=50)
url_entry.grid(column=0, row=1, sticky=(tk.W, tk.E), pady=5)

button_frame = ttk.Frame(main_frame)
button_frame.grid(column=0, row=2, sticky=(tk.W, tk.E), pady=10)
ttk.Button(button_frame, text="Fetch Playlist", command=calculate_duration).grid(column=0, row=0, padx=5)
ttk.Button(button_frame, text="Set API Key", command=set_api_key).grid(column=1, row=0, padx=5)

result_label = ttk.Label(main_frame, text="")
result_label.grid(column=0, row=3, sticky=(tk.W, tk.E), pady=5)

progress_bar = ttk.Progressbar(main_frame, mode='indeterminate')
progress_bar.grid(column=0, row=4, sticky=(tk.W, tk.E), pady=5)
progress_bar.grid_remove()

# Create a treeview for video titles with checkboxes
tree = ttk.Treeview(main_frame, columns=('Title', 'Duration'), show='headings')
tree.heading('Title', text='Title')
tree.heading('Duration', text='Duration')
tree.column('Title', width=500)
tree.column('Duration', width=100, anchor='center')
tree.grid(column=0, row=5, sticky=(tk.W, tk.E, tk.N, tk.S), pady=10)

# Add a scrollbar to the treeview
scrollbar = ttk.Scrollbar(main_frame, orient=tk.VERTICAL, command=tree.yview)
scrollbar.grid(column=1, row=5, sticky=(tk.N, tk.S))
tree.configure(yscrollcommand=scrollbar.set)

main_frame.columnconfigure(0, weight=1)
main_frame.rowconfigure(5, weight=1)

# Bind checkbox click event
tree.bind('&lt;ButtonRelease-1&gt;', toggle_watched)

# Start the GUI event loop
root.mainloop()
