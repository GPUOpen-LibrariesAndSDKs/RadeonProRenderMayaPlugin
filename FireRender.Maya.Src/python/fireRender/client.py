import urllib.request
import json
import os
from enum import Enum
from typing import Dict
from urllib.parse import (
    urlencode, unquote, urlparse, parse_qsl, urlunparse, urljoin
)


class MatlibSession:
    def __init__(self, *args, **kwargs):
        pass

    @staticmethod
    def add_url_params(url: str, params: Dict):
        """ Add GET params to provided URL being aware of existing.

        :param url: string of target URL
        :param params: dict containing requested params to be added
        :return: string with updated URL
        """
        parsed_url = urlparse(unquote(url))
        query_dict = dict(parse_qsl(parsed_url.query))
        # convert bool and dict to json strings
        query_dict = {
            k: json.dumps(v) if isinstance(v, (bool, dict)) else v
            for k, v in query_dict.items()
        }
        query_dict.update(params)
        parsed_url = parsed_url._replace(query=urlencode(query_dict, doseq=True))
        return urlunparse(parsed_url)

    @staticmethod
    def get_last_url_path(url: str):
        return urlparse(url).path.rsplit('/', 1)[-1]

    @staticmethod
    def decode_json_response(response: str):
        return json.loads(response.decode("utf-8"))


class MatlibEndpoint(Enum):
    PREFIX = 'api'
    REGISTRATION = 'registration'
    AUTH = 'auth'
    AUTH_LOGIN = 'auth/login'
    AUTH_SOCIAL = 'auth/social'
    MATERIALS = 'materials'
    CATEGORIES = 'categories'
    COLLECTIONS = 'collections'
    TAGS = 'tags'
    RENDERS = 'renders'
    PACKAGES = 'packages'


class MatlibEntityClient:
    endpoint = None
    session = None
    base = ''

    def __init__(
            self,
            session: MatlibSession,
            base: str,
            endpoint: MatlibEndpoint,
    ):
        self.session = session
        self.base = base
        self.endpoint = endpoint

    @property
    def base_url(self):
        return urljoin(self.base, '/{}/{}/'.format(MatlibEndpoint.PREFIX.value, self.endpoint.value))

    def _get_list(self, url: str = None, limit: int = None, offset: int = None, params: dict = None):
        url = urljoin(base=self.base_url, url=url)
        if params is not None:
            url = self.session.add_url_params(url, params)
        url = self.session.add_url_params(url, {'limit': limit, 'offset': offset})
        request = urllib.request.Request(url)
        response = urllib.request.urlopen(request)
        response_content = json.loads(response.read().decode("utf-8"))
        return response_content['results']

    def _get_by_id(self, item_id: str, url: str = None):
        url = urljoin(base=self.base_url, url=url)
        url = urljoin(base=url, url='{}/'.format(item_id))
        request = urllib.request.Request(url)
        response = urllib.request.urlopen(request)
        response_content = json.loads(response.read().decode("utf-8"))
        return response_content

    def _download(self, url: str, callback = None, target_dir: str = None, filename: str = None):
        response = urllib.request.urlopen(url)
        length = response.getheader('content-length')
        if length:
            length = int(length)
            blocksize = max(0x1000, length//100)
        else:
            blocksize = 1000000 # just made something up
        
        if not filename:
            filename = self.session.get_last_url_path(response.url) or 'file'
        if not target_dir:
            target_dir = '.'
        full_filename = os.path.abspath(os.path.join(target_dir, filename))
        with open(full_filename, 'wb') as file:
            size = 0
            while True:
                buf = response.read(blocksize)
                if not buf:
                    break
                file.write(buf)
                size += len(buf)
                if callback:
                    if not callback(size, length):
                        break
        if length and size != length and os.path.exists(full_filename):
            os.remove(full_filename)
            raise EOFError


class MatlibEntityListClient(MatlibEntityClient):
    def __init__(self, *args, **kwargs):
        super(MatlibEntityListClient, self).__init__(*args, **kwargs)

    def get_list(self, limit: int, offset: int, params: dict = None):
        return self._get_list(limit=limit, offset=offset, params=params)

    def get(self, item_id: str):
        return self._get_by_id(item_id=item_id)


class MatlibMaterialsClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibMaterialsClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.MATERIALS)


class MatlibCategoriesClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibCategoriesClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.CATEGORIES)


class MatlibCollectionsClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibCollectionsClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.COLLECTIONS)


class MatlibTagsClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibTagsClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.TAGS)


class MatlibRendersClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibRendersClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.RENDERS)
        self._imageurl = self.base.replace('https://api.', 'https://image.')

    def download(self, item_id: str, callback = None, target_dir: str = None, filename: str = None):
        self._download(
            url=urljoin(self.base_url, '{}/download/'.format(item_id)), callback=callback,
            target_dir=target_dir, filename=filename
        )

    def download_thumbnail(self, item_id: str, callback = None, target_dir: str = None, filename: str = None):
        self._download(
            url=urljoin(self._imageurl, '{}_thumbnail.jpeg'.format(item_id)), callback=callback, 
            target_dir=target_dir, filename=filename
        )


class MatlibPackagesClient(MatlibEntityListClient):
    def __init__(self, *args, **kwargs):
        super(MatlibPackagesClient, self).__init__(*args, **kwargs, endpoint=MatlibEndpoint.PACKAGES)

    def download(self, item_id: str, callback = None, target_dir: str = None, filename: str = None):
        self._download(
            url=urljoin(self.base_url, '{}/download/'.format(item_id)), callback=callback,
            target_dir=target_dir, filename=filename
        )


class MatlibClient:

    def __init__(self, host: str):
        """
        Web Material Library API Client
        :param host (str): Web Material Library host (example: https://web.material.library.com
        """
        self.host = host
        self.session = MatlibSession()

        self.materials = MatlibMaterialsClient(session=self.session, base=self.host)
        self.collections = MatlibCollectionsClient(session=self.session, base=self.host)
        self.categories = MatlibCategoriesClient(session=self.session, base=self.host)
        self.tags = MatlibTagsClient(session=self.session, base=self.host)
        self.renders = MatlibRendersClient(session=self.session, base=self.host)
        self.packages = MatlibPackagesClient(session=self.session, base=self.host)
