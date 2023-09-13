package config

import (
	"io/ioutil"
	"strings"

	"gopkg.in/yaml.v3"
)

var (
	collectorQATag string
)

type ImageStore struct {
	qaTag string
	Qa    map[string]string
	NonQa map[string]string `yaml:"non_qa"`
}

func (i *ImageStore) CollectorImage() string {
	return ReadEnvVar(envCollectorImage)
}

// ImageByKey looks up an image from the store, and panics
// if the image does not exist.
func (i *ImageStore) ImageByKey(key string) string {
	if img, ok := i.NonQa[key]; ok {
		return img
	}
	panic("failed to find image: " + key)
}

// QaImageByKey looks up an image from the store, and appends
// the QA tag. If the image does not exist in the store, this function
// will panic.
func (i *ImageStore) QaImageByKey(key string) string {
	img, ok := i.Qa[key]
	if ok {
		idx := strings.LastIndex(img, ":")
		img = i.qaImage(img[:idx], img[idx+1:])
		return img
	}
	panic("failed to find qa image: " + key)
}

func loadImageStore(location string) (*ImageStore, error) {
	file, err := ioutil.ReadFile(location)
	if err != nil {
		return nil, err
	}

	var store ImageStore
	err = yaml.Unmarshal(file, &store)
	if err != nil {
		return nil, err
	}

	store.qaTag = ReadEnvVar(envQATag)

	if store.qaTag == "" {
		if collectorQATag == "" {
			bytes, err := ioutil.ReadFile("container/QA_TAG")
			if err != nil {
				return nil, err
			}

			store.qaTag = strings.TrimSpace(string(bytes))
		}
	}

	return &store, nil
}

// Generate the QA tag to be used for containers by attaching the contents of
// the 'COLLECTOR_QA_TAG' environment variable if it exists. Return the base
// tag as is otherwise.
func (i *ImageStore) getQATag(base_tag string) string {
	return base_tag + "-" + i.qaTag
}

// Return the full image to be used for a QA container from a given image name
// and a tag. The tag will be adjusted accordingly to the description of
// 'getQaTag'
func (i *ImageStore) qaImage(image string, tag string) string {
	return image + ":" + i.getQATag(tag)
}
