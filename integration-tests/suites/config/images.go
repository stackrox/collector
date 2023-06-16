package config

import (
	"io/ioutil"
	"strings"

	"gopkg.in/yaml.v3"
)

type ImageStore struct {
	Images map[string]string
}

func (i *ImageStore) CollectorImage() string {
	return ReadEnvVar(envCollectorImage)
}

// ImageByKey looks up an image from the store, and panics
// if the image does not exist.
func (i *ImageStore) ImageByKey(key string) string {
	if img, ok := i.Images[key]; ok {
		return img
	}
	panic("failed to find image: " + key)
}

// QaImageByKey looks up an image from the store, and appends
// the QA tag. If the image does not exist in the store, this function
// will panic.
func (i *ImageStore) QaImageByKey(key string) string {
	img := i.ImageByKey(key)
	if img != "" {
		idx := strings.LastIndex(img, ":")
		img = qaImage(img[:idx], img[idx+1:])
	}
	return img
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

	return &store, nil
}

// Generate the QA tag to be used for containers by attaching the contents of
// the 'COLLECTOR_QA_TAG' environment variable if it exists. Return the base
// tag as is otherwise.
func getQATag(base_tag string) string {
	collector_qa_tag := ReadEnvVar(envQATag)

	if collector_qa_tag != "" {
		return base_tag + "-" + collector_qa_tag
	}
	return base_tag
}

// Return the full image to be used for a QA container from a given image name
// and a tag. The tag will be adjusted accordingly to the description of
// 'getQaTag'
func qaImage(image string, tag string) string {
	return image + ":" + getQATag(tag)
}
